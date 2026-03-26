# Cliente Web (React + Vite)

Frontend base del cliente de chat para ProyectoChat-Sistos.

## Objetivo

Esta app representa solo el lado cliente web (no host/admin) y se conecta a un WebSocket gateway que traduce eventos del navegador al protocolo TCP binario (`ChatPacket`) ya definido en el repositorio.

La pantalla de conexion solicita solo:
- Nombre de usuario
- Direccion IP del host (servidor C)
- Puerto del host (servidor C)

Con esos datos, el cliente envia al gateway el destino TCP por sesion.
El gateway WebSocket se configura por `VITE_WS_URL` (por defecto: `ws://localhost:8080/ws`).

## Requisitos

- Node.js 20+
- npm 10+

## Ejecutar

```bash
cd client/web
npm install
npm run dev
```

Build de produccion:

```bash
npm run build
npm run preview
```

## Configuracion

1. Copia `.env.example` a `.env`.
2. Ajusta valores por defecto si lo necesitas.

Ejemplo:

```env
VITE_WS_URL=ws://localhost:8080/ws
VITE_CHAT_SERVER_HOST=127.0.0.1
VITE_CHAT_SERVER_PORT=5000
```

## Contrato esperado del WebSocket gateway

### Mensajes cliente -> gateway (JSON)

```json
{ "type": "register", "username": "alice", "serverHost": "192.168.1.20", "serverPort": 5000 }
{ "type": "broadcast", "message": "hola" }
{ "type": "direct", "target": "bob", "message": "privado" }
{ "type": "list" }
{ "type": "info", "username": "bob" }
{ "type": "status", "status": "BUSY" }
{ "type": "logout" }
```

### Mensajes gateway -> cliente (JSON)

```json
{ "type": "connected", "username": "alice", "status": "ACTIVE", "message": "Bienvenido" }
{ "type": "message", "from": "alice", "scope": "broadcast", "text": "hola" }
{ "type": "user_list", "users": [{ "username": "alice", "status": "ACTIVE" }] }
{ "type": "user_info", "user": { "username": "bob", "ip": "192.168.1.10", "status": "BUSY" } }
{ "type": "status_changed", "status": "INACTIVE" }
{ "type": "error", "message": "Usuario no existe" }
```

## Mapeo sugerido hacia `ChatPacket`

- `register` -> `CMD_REGISTER`
- `broadcast` -> `CMD_BROADCAST`
- `direct` -> `CMD_DIRECT`
- `list` -> `CMD_LIST`
- `info` -> `CMD_INFO`
- `status` -> `CMD_STATUS`
- `logout` -> `CMD_LOGOUT`

Y desde servidor:

- `CMD_MSG` -> `message`
- `CMD_USER_LIST` -> `user_list`
- `CMD_USER_INFO` -> `user_info`
- `CMD_OK` (registro/status) -> `connected` o `status_changed`
- `CMD_ERROR` -> `error`

## Pantallas incluidas

- Formulario de conexion con usuario, IP del host C y puerto del host C
- Sidebar con:
	- Nombre de usuario e IP propia
	- Estado actual (badge con color)
	- Dropdown para cambiar estado
	- Lista de chats: General y privados por usuario conectado
	- Boton manual "Actualizar" para refrescar usuarios
- Panel de chat principal:
	- Chat General (solo broadcast)
	- Chat privado por usuario (solo mensajes directos de ese chat)
- Panel separado de "Sistema" para eventos del servidor
- Composer de envio contextual segun chat seleccionado
- Manejo visual de conexion: `offline`, `connecting`, `online`

## Comportamiento actual de refresh

- No existe auto-refresh periodico.
- El refresh ocurre cuando:
	- Se completa el registro inicial
	- Se presiona el boton manual "Actualizar"

## Notas

- El frontend puede correr con un gateway mock mientras se implementa el puente real C<->WebSocket.
- El limite de input para mensaje se alinea al payload del protocolo (957 bytes maximo).
- Si el servidor responde "IP ya conectada", el cliente bloquea el ingreso y permanece en login.
