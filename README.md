# Master of Files - Sistema Distribuido de Gestión de Archivos

**Master of Files** es un proyecto desarrollado para la cátedra de Sistemas Operativos (UTN-FRBA) que consiste en un sistema distribuido capaz de gestionar archivos y ejecutar consultas (queries) de forma eficiente. El sistema implementa conceptos avanzados de planificación, gestión de memoria y persistencia de datos en un entorno multimodular.

---

## Arquitectura del Sistema

El sistema está compuesto por cuatro módulos independientes que se comunican mediante sockets:

| Módulo | Función | Responsabilidad Técnica |
| :--- | :--- | :--- |
| **Query Control (QC)** | Interfaz de Usuario | Cliente que envía scripts de consultas y gestiona la prioridad de ejecución. |
| **Master** | Kernel | Administra la conexión de Workers, planifica las tareas y maneja el flujo de datos. |
| **Worker** | Unidad de Cómputo | Interpreta instrucciones, gestiona una memoria caché con paginación y ejecuta las operaciones. |
| **Storage** | Persistencia (FS) | Gestiona el almacenamiento físico, estructuras de control (Bitmap/Superbloque) e integridad (Hashing). |

---

## Conceptos de Sistemas Operativos Aplicados

### 1. Planificación (Scheduling)
El módulo **Master** actúa como el planificador de largo y corto plazo, implementando:
* **FIFO (First-In-First-Out):** Procesamiento por orden de llegada.
* **Prioridades con Desalojo:** Interrupción de tareas en ejecución ante la llegada de procesos más críticos.
* **Aging (envejecimiento):** Prevención de la inanición (*starvation*) mediante el incremento dinámico de la prioridad de tareas en espera.

### 2. Gestión de Memoria
Cada **Worker** administra su propia memoria interna simulando una jerarquía de memoria:
* **Paginación a Demanda:** Carga de datos solo cuando la instrucción lo requiere.
* **Algoritmos de Reemplazo:** Gestión de espacio mediante **LRU** (Least Recently Used) y **CLOCK-M** (Reloj Modificado).
* **Coherencia de Datos:** Implementación de *Dirty Bit* (bit de modificación) para sincronizar cambios con el Storage solo cuando es necesario (Write-back).

### 3. Sistema de Archivos (File Systems)
El **Storage** emula un File System con las siguientes características:
* **Estructuras Administrativas:** Manejo de un *Superbloque* para configuración y un *Bitmap* para la gestión de bloques libres/ocupados.
* **Persistencia Real:** Almacenamiento binario de bloques y gestión de metadata por cada archivo y tag.
* **Integridad:** Generación de índices basados en **Hash** para cada bloque de datos almacenado.

### 4. Concurrencia y Comunicación
* **Sockets de Red:** Comunicación inter-proceso (IPC) en un entorno distribuido.
* **Manejo de Contextos:** Capacidad de interrumpir una tarea, guardar su estado y retomarla posteriormente sin pérdida de datos.

---

## Dependencias

Para poder compilar y ejecutar el proyecto, es necesario tener instalada la
biblioteca [so-commons-library] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-commons-library
cd so-commons-library
make debug
make install
```

## Compilación y ejecución

Cada módulo del proyecto se compila de forma independiente a través de un
archivo `makefile`. Para compilar un módulo, es necesario ejecutar el comando
`make` desde la carpeta correspondiente.

El ejecutable resultante de la compilación se guardará en la carpeta `bin` del
módulo. Ejemplo:

```sh
cd kernel
make
./bin/kernel
```

## Importar desde Visual Studio Code

Para importar el workspace, debemos abrir el archivo `tp.code-workspace` desde
la interfaz o ejecutando el siguiente comando desde la carpeta raíz del
repositorio:

```bash
code tp.code-workspace
```

## Checkpoint

Para cada checkpoint de control obligatorio, se debe crear un tag en el
repositorio con el siguiente formato:

```
checkpoint-{número}
```

Donde `{número}` es el número del checkpoint, ejemplo: `checkpoint-1`.

Para crear un tag y subirlo al repositorio, podemos utilizar los siguientes
comandos:

```bash
git tag -a checkpoint-{número} -m "Checkpoint {número}"
git push origin checkpoint-{número}
```

> [!WARNING]
> Asegúrense de que el código compila y cumple con los requisitos del checkpoint
> antes de subir el tag.

## Entrega

Para desplegar el proyecto en una máquina Ubuntu Server, podemos utilizar el
script [so-deploy] de la cátedra:

```bash
git clone https://github.com/sisoputnfrba/so-deploy.git
cd so-deploy
./deploy.sh -r=release -p=utils -p=query_control -p=master -p=worker -p=storage "tp-{año}-{cuatri}-{grupo}"
```

El mismo se encargará de instalar las Commons, clonar el repositorio del grupo
y compilar el proyecto en la máquina remota.

> [!NOTE]
> Ante cualquier duda, pueden consultar la documentación en el repositorio de
> [so-deploy], o utilizar el comando `./deploy.sh --help`.

## Guías útiles

- [Cómo interpretar errores de compilación](https://docs.utnso.com.ar/primeros-pasos/primer-proyecto-c#errores-de-compilacion)
- [Cómo utilizar el debugger](https://docs.utnso.com.ar/guias/herramientas/debugger)
- [Cómo configuramos Visual Studio Code](https://docs.utnso.com.ar/guias/herramientas/code)
- **[Guía de despliegue de TP](https://docs.utnso.com.ar/guías/herramientas/deploy)**

[so-commons-library]: https://github.com/sisoputnfrba/so-commons-library
[so-deploy]: https://github.com/sisoputnfrba/so-deploy
