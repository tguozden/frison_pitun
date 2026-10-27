# Frisón: notas del gateway LoRa + raspi + WeeWX

**Fecha:** 27 de octubre de 2026

## Cambios a aplicar

1. **`fakewunder.py`: corregir el módulo del offset.** Es la falla principal; ver más abajo.
   ```python
   seconds_offset = round(datetime.timestamp(datetime.now())) % 3600   # antes: % 360
   ```
2. **`weewx.conf`: agregar `stale = 3600` en `[[Wunderground]]`.** Descarta los registros con más de 1 h de antigüedad, así que solo sale el de la última hora aunque el catch-up traiga varios.
   ```
   [StdRESTful]
       [[Wunderground]]
           ...
           stale = 3600
   ```
   - Se puede aplicar junto con el punto 1. Hoy el sistema está trabado en ~HH:55, y en ese primer ciclo el registro tiene ~55 min, así que pasa. Con 3000 se descartaría.
   - `stale` filtra, no fuerza: si no hay ningún registro nuevo de la última hora, no se manda nada y el ESP vuelve a `tsleep = 3600`.

**Sin cambios:**
- ESP: `+60`, `TSLEEP = 3600`, `NBOOTS = 10` y la ventana de 45 s.
- WeeWX: `StdTimeSynch` modificado en `prep_services`, `record_generation = hardware` y `restx.py` como está.

Se revisaron los cuatro componentes de la cadena:
- **ESP:** `frison_gateway.ino` y `frison_aux.h`.
- **WeeWX:** `weewx.conf`, `engine.py` (modificado), `restx.py` (modificado) y `vantage.py`.
- **Raspi:** `fakewunder.py`.

---

## Falla principal: `% 360` en `fakewunder.py`

```python
seconds_offset = round(datetime.timestamp(datetime.now()))%360   # quedó del simulador
```

Con módulo 360, el ESP solo sabe en qué parte de un **bloque de 6 min** está, no en qué parte de la hora. Llamemos r a la hora en que la raspi manda el paquete y K al overhead del ciclo:

```
r  = 360·q + (r mod 360)
r' = 360·(q + 10) + 60 + K
```

- Dentro del bloque converge a 60 + K.
- **El bloque q (mod 10) no cambia nunca.** La hora tiene 10 bloques y solo el primero (HH:00–06) es el correcto.

Cualquier perturbación (primer arranque, un "no llegó mensaje", un ciclo salteado por batería) deja el sistema **trabado para siempre** en el bloque donde le toque. Por ejemplo, trabado en las HH:50:
1. Manda el registro de las HH:00 con 50 min de atraso. Desde afuera parece "el dato de la hora anterior".
2. No entra en la condición de 5 min de `fakewunder.py`, así que no hace `shutdown` y el ESP le corta Vext con el sistema andando.

**Con `% 3600`:** r' mod 3600 = 60 + K, así que converge a ~HH:02 en un solo ciclo desde cualquier fase.

`epoch % 3600` coincide con los segundos pasados de la hora local porque UTC−3 es un número entero de horas. La raspi tiene la hora de la consola, que es la misma referencia que usa el logger.

---

## Cadena completa

```
Consola Davis --serie--> WeeWX (raspi) --HTTP WU--> fakewunder.py --UART 26 bytes--> ESP --LoRa--> receptor
```

1. **El ESP prende la raspi.** Lo hace 1 de cada 10 despertares y la espera 17,5 s.
2. **WeeWX arranca:**
   1. `StdTimeSynch` (modificado) copia la hora de la consola a la raspi.
   2. Hace el catch-up del logger de la consola.
   3. Postea cada registro por protocolo Wunderground a `fakewunder.py` (`192.168.4.1`).
3. **`fakewunder.py`** arma el paquete de 26 bytes, lo manda por UART al ESP y, si el registro tiene menos de 5 min, hace `shutdown now`.
4. **El ESP:**
   1. Recibe el paquete.
   2. Calcula el `tsleep`.
   3. Pone el voltaje y el checksum.
   4. Lo manda por LoRa.
   5. A los 3 s corta Vext.

   Está probado que la raspi termina de apagarse en ese tiempo.

---

## ESP (`frison_gateway.ino` + `frison_aux.h`)

### Ciclo base
- `TSLEEP = 3600`, `NBOOTS = 10`.
- El ESP duerme `tsleep/NBOOTS` segundos por vez (≈360 s = 6 min). En cada sueño, `rtc_data.bootCount++` (en `apaga()`).
- **Ciclo con raspi** (`bootCount % NBOOTS == 0`):
  1. Prende Vext.
  2. Espera 17,5 s.
  3. Abre `Serial1` y espera el paquete de 26 bytes.
- **Ventana de espera:** `millis0 = millis() + 45000` se evalúa al inicializar el programa, así que son 45 s **desde el boot**. Incluyen los 17,5 s de espera. Si llega el paquete, la ventana se acorta a 3 s.
- **Ciclo sin raspi** (los otros 9): espera 2 s y reenvía el último `txpacket` guardado en RTC. Solo actualiza el voltaje (byte 24) y recalcula el checksum. **El timestamp queda igual en los 9 reenvíos.**

### Ajuste del offset, línea por línea (`onReceiveFunction`)

```c
memcpy(&seconds_25, &txpacket[0], 4);   // timestamp del registro (s desde 2025-01-01 UTC)
seconds_25 -= seconds_25 % 60;          // redondeo al minuto (etapa de simulación)
memcpy(&txpacket[0], &seconds_25, 4);   // lo vuelve a escribir en el paquete

memcpy(&seconds_offset, &txpacket[24], 2); // bytes 24-25: segundos pasados de la hora (de fakewunder.py)
tsleep = TSLEEP - seconds_offset + 60;      // duración total hasta el próximo ciclo con raspi

txpacket[24] = (uint8_t)(0.255 * (voltaje - 3300)); // pisa el byte 24 con el voltaje
txpacket[25] = calcular_checksum_xor(txpacket, 25);  // checksum nuevo
```

**Cuánto duerme hasta el próximo envío.** K es el overhead: los 9 despertares sin raspi (≈2–3 s cada uno), más los 3 s de espera, más el tiempo desde que despierta el ESP hasta que la raspi manda (booteo + WeeWX + catch-up).

```
r' = r + (3600 − offset + 60) + K
```

Con `offset = r mod 3600`, queda:

```
r' mod 3600 = 60 + K
```

O sea, **siempre apunta a ~HH:02**.

- **No es un lazo acumulativo.** Cada hora corrige desde cero con el offset que acaba de medir.
- **Valor mínimo de `tsleep`:** si la raspi manda justo antes de la hora, el offset ≈ 3599 y `tsleep` ≈ 61 s. El próximo ciclo con raspi llega a los ~2 min y agarra el registro nuevo. No puede quedar negativo.
- **Si no llega el paquete:** `tsleep = 3600` sin corrección. La fase deriva con el overhead hasta que un ciclo vuelve a recibir el paquete, y ahí converge.
- **Referencia de tiempo:** toda la cadena usa la hora de la **consola**. Que la consola esté atrasada respecto de la hora real no afecta la alineación.

### Casos especiales
- **Primer arranque** (magic distinto): `bootCount = 0`, raspi prendida, `tsleep = 3600` y timestamp en 0.
- **Voltaje < 3800 mV:** parpadea y se duerme, pero `bootCount` igual incrementa. Si cae en un ciclo con raspi, esa lectura se pierde hasta 10 ciclos después. La fase horaria no cambia.

### División entera (detalle menor)
`tsleep/NBOOTS*1000*(uint64_t)1000`: C divide primero entre `int` y descarta los decimales. Por ejemplo, 3655/10 = 365, así que se pierden hasta `NBOOTS − 1` segundos por hora. El offset lo compensa en el ciclo siguiente.

Si se quiere exacto, es opcional:
```c
esp_sleep_enable_timer_wakeup((uint64_t)tsleep * 1000000 / NBOOTS);
```

### Si se cambia `NBOOTS`
La lógica del offset no depende de `NBOOTS`: la suma de los sueños sigue siendo `tsleep`. Solo cambia la cantidad de reenvíos LoRa por hora y el intervalo entre ellos. Por ejemplo, con 9 quedan 8 reenvíos, cada 6:40.

---

## WeeWX en la raspi

- **Driver:** Vantage Pro2 por serie (`/dev/ttyUSB0`), con `record_generation = hardware`.
- **`archive_interval`:** el `300` del conf **se ignora**; se usa el de la consola (1 h). Está confirmado en el código:
  - En `vantage.py`, `_setup()` lee `archive_interval_` de la EEPROM de la consola (dirección 0x2D × 60), y la propiedad `archive_interval` devuelve ese valor.
  - En `engine.py`, `StdArchive.__init__` con `hardware` usa el valor del driver. El del conf solo se usa con `software`, o si el driver no implementa la propiedad.
- **`StdTimeSynch` modificado en `engine.py`:** al arrancar lee la hora de la consola con `getTime()` y la pone en la raspi con `sudo date -s`.
  - **Nunca llama a `setTime()`, así que la raspi no modifica la hora de la consola.**
  - `clock_check` y `max_drift` del conf no se usan.
  - **Tiene que quedar en `prep_services`.** Así corre antes del catch-up de `StdArchive`, y tanto el filtro de registros futuros del catch-up como `stale` comparan contra la hora correcta.
- **Catch-up** (`StdArchive._catchup` y `Vantage.genArchiveRecords`): trae todos los registros posteriores al último de la base, con el comando DMPAFT, y solo descarta los que tienen timestamp en el futuro. Si la consola ya grabó el registro de las HH:00 cuando arranca WeeWX, viene seguro.
- **Uploader** (`StdWunderground`, en `restx.py`): publica todos los registros que recibe, en orden, del más viejo al más nuevo. Sin `stale` incluye también los viejos del catch-up.
  - `stale` se lee del conf: `get_site_dict` → `AmbientThread` → `RESTThread`.
  - Se aplica en `skip_this_post()`, que compara `time.time() − dateTime` contra `stale`.

---

## `fakewunder.py`

- Recibe el GET del protocolo WU, arma el paquete con `struct.pack('IeeBBHeeHeeeH', ...)` (26 bytes) y lo escribe por `/dev/ttyAMA0`.
- `dateutc` viene en UTC y se convierte a segundos desde 2025-01-01 UTC.
- **`ser.write` se ejecuta para todos los registros que llegan**, antes del chequeo de los 5 min. Por eso conviene `stale` en WeeWX.
- El `shutdown` se hace solo si el registro tiene menos de 5 min (`ultimo + 5 min > now`). Con +60 en el ESP y `% 3600`, el envío cae en ~HH:02, así que entra.

---

## Respaldo

Una actualización de WeeWX pisa los cambios hechos en sus archivos:
- `engine.py`: `StdTimeSynch` reescrito y `StdCalibrate`, que saltea `foo`.
- `restx.py`: `pws_url` apuntando a `192.168.4.1`.