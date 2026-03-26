# Gateway WebSocket <-> Chat TCP (C)

Este servicio permite que el cliente web (navegador) hable con el servidor de chat en C (`ChatPacket` binario de 1024 bytes).

## Por que existe

El navegador no puede abrir sockets TCP binarios directos al servidor C. Solo puede usar WebSocket/HTTP.

## Ejecutar

```bash
cd client/gateway
npm install
npm start
```

## Variables de entorno

Copia `.env.example` a `.env` si deseas cambiar defaults.

- `WS_PORT`: puerto donde escucha el gateway WebSocket
- `WS_PATH`: path WebSocket (default `/ws`)
- `DEFAULT_CHAT_SERVER_HOST`: host por defecto del servidor C
- `DEFAULT_CHAT_SERVER_PORT`: puerto por defecto del servidor C

## Flujo esperado

1. Frontend se conecta al gateway (`ws://localhost:8080/ws`).
2. Frontend envia `register` con:
   - `username`
   - `serverHost`
   - `serverPort`
3. Gateway abre una conexion TCP al host C indicado.
4. Gateway traduce JSON <-> ChatPacket.

## Mensajes frontend -> gateway

```json
{ "type": "register", "username": "alice", "serverHost": "192.168.1.20", "serverPort": 5000 }
{ "type": "broadcast", "message": "hola" }
{ "type": "direct", "target": "bob", "message": "privado" }
{ "type": "list" }
{ "type": "info", "username": "bob" }
{ "type": "status", "status": "BUSY" }
{ "type": "logout" }
```

## Mensajes gateway -> frontend

```json
{ "type": "connected", "username": "alice", "status": "ACTIVE", "message": "Bienvenido alice" }
{ "type": "message", "from": "alice", "scope": "broadcast", "text": "hola" }
{ "type": "user_list", "users": [{ "username": "alice", "status": "ACTIVE" }] }
{ "type": "user_info", "user": { "username": "bob", "ip": "192.168.1.10", "status": "BUSY" } }
{ "type": "status_changed", "status": "INACTIVE" }
{ "type": "error", "message": "Destinatario no conectado" }
```

## Nota importante sobre IP duplicada

Tu servidor C actual rechaza usuarios con IP repetida. Si varios clientes usan el mismo gateway remoto centralizado, el servidor C podria ver una sola IP y rechazar registros adicionales.

Para evitarlo, ejecuta el gateway localmente en cada cliente, o elimina en el servidor C la restriccion de IP duplicada si asi lo permite su diseno.
