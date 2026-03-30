# FFmpeg GUI Spec

## 1. Proposito

Este documento describe el estado actual de `ffmpeg-gui`, lo que ya resuelve hoy, que conviene endurecer sobre la base existente y que funcionalidades todavia faltan para considerarlo un wrapper GUI de FFmpeg mas completo y solido.

La meta del proyecto sigue siendo:

- configurar tareas comunes de FFmpeg desde una GUI
- mostrar el comando equivalente en tiempo real
- permitir copiarlo o exportarlo
- ejecutar el proceso desde la app
- mostrar logs, estado y progreso

No es objetivo construir un editor de video complejo ni intentar cubrir todo FFmpeg.

## 2. Estado actual del proyecto

### 2.1 Nivel de madurez

El proyecto ya paso el punto de "esqueleto MVP". Hoy tiene un flujo real de punta a punta y varias capacidades de wrapper que antes eran backlog:

- compila en Linux con CMake
- tiene separacion clara por capas
- permite ejecutar conversiones reales
- ya incorpora validacion, estado de proceso y progreso
- ya agrega presets, export de script, historial y ruta manual de `ffmpeg`

La base ya es suficiente para iterar sin rehacer arquitectura.

### 2.2 Arquitectura actual

La separacion principal sigue siendo correcta:

- `src/core/`
  `FFmpegJob` representa la intencion del usuario. `JobValidator` centraliza reglas, errores y warnings.
- `src/command/`
  `CommandBuilder` genera tanto la representacion visible del comando como la lista de argumentos ejecutables.
- `src/process/`
  `FFmpegProcess` maneja spawn, estado, cancelacion y chequeo de disponibilidad. `ProgressParser` interpreta la salida de FFmpeg para poblar progreso.
- `src/gui/`
  `MainFrame` concentra la UI wxWidgets, construye `FFmpegJob` desde widgets y muestra preview, historial, logs y progreso.

La regla de diseno principal se mantiene: la GUI no ejecuta una string cruda ni decide flags finales de FFmpeg por si sola.

## 3. Que ya puede hacer hoy

### 3.1 Flujos funcionales implementados

Hoy la app soporta:

- seleccionar input file
- seleccionar output file
- elegir contenedor de salida
- elegir ruta manual del binario `ffmpeg`
- aplicar presets rapidos
- elegir codec de video
- elegir codec de audio
- usar `copy` de video o audio
- extraer audio solamente con `-vn`
- elegir CRF o bitrate de video
- elegir bitrate de audio
- elegir resolucion por presets
- trim por `start` y `end` o por `start` y `duration`
- agregar `extra args`
- ver el comando generado en tiempo real
- copiar el comando
- exportar el comando como script
- ejecutar FFmpeg
- cancelar el proceso
- ver logs
- ver estado final del proceso
- ver progreso parseado desde stderr
- consultar historial reciente de comandos de la sesion

### 3.2 Mejoras nuevas respecto al spec anterior

El proyecto avanzo en puntos que antes seguian marcados como futuros:

- la GUI ahora expone `ffmpegPath`
- la GUI ahora expone `extraArgs`
- hay presets listos para flujos comunes
- hay export a `.sh` o `.bat`
- hay historial reciente en memoria
- el trim ahora se modela mejor desde la UI con modos excluyentes
- el quoting visible del comando es bastante mejor
- la validacion ahora revisa extension/contenedor y tiempos reales

## 4. Estado contra el brief original

| Requisito | Estado | Nota |
|---|---|---|
| Input / output file picker | Cumplido | Implementado |
| Seleccion de contenedor de salida | Cumplido | Existe en UI y validacion |
| Codec de video | Cumplido | Incluye `copy` |
| Codec de audio | Cumplido | Incluye `copy` |
| CRF o bitrate | Cumplido | Implementado |
| Resolucion | Cumplido | Por presets |
| Trim basico | Cumplido | `start`, `end`, `duration` |
| Extraccion de audio | Cumplido | Modo audio-only |
| Comando visible y copiable | Cumplido | Preview actualizado en tiempo real |
| Ejecucion con logs | Cumplido | Flujo real de punta a punta |
| Estado de ejecucion | Cumplido | Estado visible en UI |
| Cancelacion | Cumplido | Graciosa + fallback |
| Progreso basico | Cumplido | Parseado desde stderr |
| Deteccion de FFmpeg | Cumplido | Chequeo previo a ejecutar |
| Presets rapidos | Cumplido | Implementados en GUI |
| Extra args | Cumplido | Expuestos en GUI |
| Historial | Parcial | Existe, pero solo en memoria y a nivel comando |
| Export a script | Cumplido | `.sh` / `.bat` |
| Arquitectura separada | Cumplido | `core`, `command`, `process`, `gui` |
| Compatibilidad Windows | Parcial | Pensado desde el diseno, no verificado en uso real |

## 5. Que conviene mejorar ahora

Esta seccion ya no es sobre agregar features por sumar, sino sobre endurecer lo que existe.

### 5.1 Hacer que el contenedor mande de verdad

Hoy el selector de contenedor:

- sincroniza la extension del output
- participa en la validacion
- influye en presets y warnings

Pero todavia no gobierna completamente el comando ejecutable. Mejora recomendada:

- decidir si el builder debe emitir `-f <container>`
- centralizar reglas codec/contenedor
- evitar combinaciones inconsistentes antes de ejecutar

### 5.2 Mejorar el parseo de `extra args`

Hoy `extra args` se tokeniza por espacios desde la GUI. Eso es util, pero limitado. Problemas actuales:

- no preserva argumentos con espacios
- no respeta quoting manual del usuario
- puede romper flags con valores compuestos

Si esta feature se va a conservar, conviene parsearla de forma mas rigurosa o exponerla como lista estructurada.

### 5.3 Persistir historial y presets

El historial actual sirve, pero es solo en memoria y guarda strings de comando, no jobs. Los presets actuales tambien son hardcoded. Mejora recomendada:

- guardar historial entre sesiones
- permitir presets propios
- guardar configuraciones como `FFmpegJob`

### 5.4 Endurecer aun mas el quoting visible

El comando visible mejoro bastante con `ShellQuote()`, pero sigue siendo una representacion de conveniencia. Conviene seguir mejorandolo para:

- paths raros
- diferencias POSIX/Windows
- fidelity del comando exportado

La ejecucion real ya esta bien porque usa argumentos estructurados.

### 5.5 Refinar `CheckAvailable`

La deteccion previa de FFmpeg ya contempla paths con espacios, pero sigue usando un comando string sincronico. Conviene revisar:

- consistencia con el resto del enfoque sin shell
- comportamiento real en Windows
- posible cache del resultado

### 5.6 Reforzar progreso y observabilidad

El progreso actual es util, pero depende de stderr textual. Todavia faltan:

- metadata del input
- ETA confiable
- distincion mas rica entre warning, error y progreso
- progreso robusto en casos menos comunes

## 6. Que podemos agregar

Estas son capacidades nuevas o seminuevas que ya tienen sentido sobre la base actual.

### 6.1 De alto valor y bajo riesgo

1. Persistencia de historial.
2. Presets guardables por el usuario.
3. Mejor parser para `extra args`.
4. Opcion para exportar tambien el job, no solo el comando.
5. Validacion mas rica de combinaciones codec/contenedor.

### 6.2 De alto valor de producto

1. Integracion con `ffprobe`.
2. Panel de metadata del archivo de entrada.
3. Resolucion custom ademas de presets.
4. Opcion "sin audio" para salida de video.
5. Opcion overwrite configurable.
6. Sugerencias inteligentes para nombre de salida.

### 6.3 Para una version mas pro

1. Cola de trabajos.
2. Persistencia completa de configuraciones.
3. Jobs recientes reeditables.
4. Perfiles avanzados por codec.
5. Packaging / instaladores.

## 7. Gaps importantes que siguen abiertos

### 7.1 Pruebas automatizadas

Sigue faltando cobertura para:

- `JobValidator`
- `CommandBuilder`
- `ProgressParser`
- reglas de trim
- quoting del comando visible
- reglas de contenedor

Esta sigue siendo una de las deudas mas importantes del repo.

### 7.2 Portabilidad real

Windows sigue estando cubierto sobre todo por diseno. Falta validacion real de:

- spawn del proceso
- cancelacion
- quoting de rutas
- `CheckAvailable`
- export de scripts
- progreso y logs

### 7.3 Persistencia

El proyecto ya tiene historial, presets y export, pero sin persistencia real:

- historial solo en memoria
- presets hardcoded
- configuraciones no reeditables

### 7.4 Inteligencia del wrapper

Todavia no hay una capa que entienda realmente el input. Falta:

- metadata via `ffprobe`
- reglas mas completas de codec/contenedor
- mejores sugerencias UX segun el archivo real

## 8. Roadmap recomendado

### Fase 1: endurecer la implementacion actual

- mejorar parseo de `extra args`
- decidir politica de contenedor y `-f`
- revisar `CheckAvailable`
- agregar tests para `core/`, `command/` y `process/`
- sincronizar docs y UX menor

### Fase 2: hacer persistente lo util

- historial persistente
- presets guardables
- configuracion de usuario persistente
- jobs reeditables

### Fase 3: volverlo mas inteligente

- integrar `ffprobe`
- mostrar metadata del input
- mejorar reglas codec/contenedor
- sumar resolucion custom y opcion sin audio

### Fase 4: preparar una v1 fuerte

- validar Windows en serio
- packaging
- polishing UX
- documentacion de usuario

## 9. Criterio de aceptacion para una v1 fuerte

Se puede considerar que el proyecto llego a una v1 fuerte si cumple esto:

- mantiene la separacion GUI / dominio / command / process
- permite transcode, copy, audio-only, presets y trim con UX clara
- soporta `extra args` de forma segura y entendible
- el contenedor elegido tiene efecto consistente y validado
- muestra comando, logs, estado y progreso confiables
- permite exportar y reusar trabajos o scripts
- tiene tests en las capas sin GUI
- esta validado en Linux y Windows

## 10. Resumen ejecutivo

`ffmpeg-gui` ya no es solo un MVP basico. Hoy ya incorpora varias funciones que acercan el producto a un wrapper usable de verdad: presets, historial, export de script, ruta manual de `ffmpeg`, `extra args`, validacion mas fuerte y mejor representacion visible del comando. La prioridad ahora deberia pasar de "sumar controles" a endurecer lo implementado, agregar persistencia, sumar metadata real del input y validar la portabilidad en Windows.
