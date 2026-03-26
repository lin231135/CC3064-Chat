import net from "node:net";
import { WebSocketServer } from "ws";

const CMD = {
  REGISTER: 1,
  BROADCAST: 2,
  DIRECT: 3,
  LIST: 4,
  INFO: 5,
  STATUS: 6,
  LOGOUT: 7,
  OK: 8,
  ERROR: 9,
  MSG: 10,
  USER_LIST: 11,
  USER_INFO: 12,
  DISCONNECTED: 13
};

const STATUS = new Set(["ACTIVE", "BUSY", "INACTIVE"]);

const PACKET_SIZE = 1024;
const SENDER_SIZE = 32;
const TARGET_SIZE = 32;
const PAYLOAD_SIZE = 957;

const WS_PORT = Number.parseInt(process.env.WS_PORT || "8080", 10);
const WS_PATH = process.env.WS_PATH || "/ws";
const DEFAULT_CHAT_SERVER_HOST = process.env.DEFAULT_CHAT_SERVER_HOST || "127.0.0.1";
const DEFAULT_CHAT_SERVER_PORT = Number.parseInt(process.env.DEFAULT_CHAT_SERVER_PORT || "5000", 10);

function fixedString(buffer, offset, size) {
  const raw = buffer.subarray(offset, offset + size);
  const zeroIndex = raw.indexOf(0x00);
  const end = zeroIndex >= 0 ? zeroIndex : raw.length;
  return raw.subarray(0, end).toString("utf8");
}

function writeString(buffer, offset, size, value) {
  const encoded = Buffer.from((value || "").toString(), "utf8");
  encoded.copy(buffer, offset, 0, Math.min(size, encoded.length));
}

function buildPacket({ command, sender = "", target = "", payload = "" }) {
  const buf = Buffer.alloc(PACKET_SIZE);
  const payloadText = payload.toString().slice(0, PAYLOAD_SIZE);
  const payloadBytes = Buffer.from(payloadText, "utf8");

  buf.writeUInt8(command, 0);
  // C struct serializa uint16_t en little-endian en x86/x64 Linux.
  buf.writeUInt16LE(Math.min(payloadBytes.length, PAYLOAD_SIZE), 1);
  writeString(buf, 3, SENDER_SIZE, sender);
  writeString(buf, 35, TARGET_SIZE, target);
  payloadBytes.copy(buf, 67, 0, Math.min(payloadBytes.length, PAYLOAD_SIZE));

  return buf;
}

function parsePacket(buf) {
  const payloadLen = Math.min(buf.readUInt16LE(1), PAYLOAD_SIZE);
  return {
    command: buf.readUInt8(0),
    sender: fixedString(buf, 3, SENDER_SIZE),
    target: fixedString(buf, 35, TARGET_SIZE),
    payload: buf.subarray(67, 67 + payloadLen).toString("utf8")
  };
}

function parseUserList(payload) {
  return payload
    .split(";")
    .filter(Boolean)
    .map((entry) => {
      const [username = "", status = "ACTIVE"] = entry.split(",");
      return { username, status };
    });
}

function parseUserInfo(payload, requestedUsername) {
  const [ip = "", status = "ACTIVE"] = payload.split(",");
  return {
    username: requestedUsername || "",
    ip,
    status
  };
}

function toClientEvent(packet, session) {
  switch (packet.command) {
    case CMD.OK:
      if (packet.payload.startsWith("Bienvenido")) {
        return {
          type: "connected",
          username: session.username,
          status: "ACTIVE",
          message: packet.payload
        };
      }
      if (STATUS.has(packet.payload)) {
        return {
          type: "status_changed",
          status: packet.payload
        };
      }
      return {
        type: "message",
        from: "SERVER",
        scope: "system",
        text: packet.payload
      };
    case CMD.ERROR:
      return {
        type: "error",
        message: packet.payload || "Error del servidor"
      };
    case CMD.MSG:
      if ((packet.sender || "").toUpperCase() === "SERVER") {
        return {
          type: "message",
          from: "SERVER",
          scope: "system",
          text: packet.payload,
          target: packet.target || "ALL"
        };
      }

      return {
        type: "message",
        from: packet.sender || "SERVER",
        scope: packet.target === "ALL" ? "broadcast" : "direct",
        text: packet.payload,
        target: packet.target || "ALL"
      };
    case CMD.USER_LIST:
      return {
        type: "user_list",
        users: parseUserList(packet.payload)
      };
    case CMD.USER_INFO:
      {
        const requestedUsername = session.pendingInfoTargets.shift() || "";
        return {
          type: "user_info",
          user: parseUserInfo(packet.payload, requestedUsername)
        };
      }
    case CMD.DISCONNECTED:
      return {
        type: "message",
        from: "SERVER",
        scope: "system",
        text: `${packet.payload} se desconecto`
      };
    default:
      return {
        type: "message",
        from: packet.sender || "SERVER",
        scope: "system",
        text: packet.payload
      };
  }
}

function sendJson(ws, payload) {
  if (ws.readyState === ws.OPEN) {
    ws.send(JSON.stringify(payload));
  }
}

function sendPacket(session, packet) {
  if (!session.tcp || session.tcp.destroyed) {
    throw new Error("No hay conexion TCP activa con el servidor C");
  }
  session.tcp.write(packet);
}

function startTcpSession(session, host, port) {
  return new Promise((resolve, reject) => {
    const tcp = net.createConnection({ host, port });

    const onError = (error) => {
      reject(error);
    };

    tcp.once("error", onError);

    tcp.once("connect", () => {
      tcp.removeListener("error", onError);
      session.tcp = tcp;
      session.tcpBuffer = Buffer.alloc(0);

      tcp.on("data", (chunk) => {
        session.tcpBuffer = Buffer.concat([session.tcpBuffer, chunk]);

        while (session.tcpBuffer.length >= PACKET_SIZE) {
          const packetBuffer = session.tcpBuffer.subarray(0, PACKET_SIZE);
          session.tcpBuffer = session.tcpBuffer.subarray(PACKET_SIZE);
          const packet = parsePacket(packetBuffer);
          sendJson(session.ws, toClientEvent(packet, session));
        }
      });

      tcp.on("close", () => {
        sendJson(session.ws, { type: "error", message: "Conexion TCP cerrada por el host" });
        if (session.ws.readyState === session.ws.OPEN) {
          session.ws.close(1011, "host-disconnected");
        }
      });

      tcp.on("error", (error) => {
        sendJson(session.ws, { type: "error", message: `Error TCP: ${error.message}` });
      });

      resolve();
    });
  });
}

function closeTcp(session) {
  if (session.tcp && !session.tcp.destroyed) {
    try {
      session.tcp.write(buildPacket({ command: CMD.LOGOUT, sender: session.username }));
    } catch {
      // ignore send failures on shutdown
    }
    session.tcp.end();
    session.tcp.destroy();
  }
  session.tcp = null;
  session.tcpBuffer = Buffer.alloc(0);
  session.pendingInfoTargets = [];
}

async function handleClientMessage(session, rawData) {
  let msg;

  try {
    msg = JSON.parse(rawData.toString());
  } catch {
    sendJson(session.ws, { type: "error", message: "JSON invalido" });
    return;
  }

  if (msg.type === "register") {
    const username = (msg.username || "").trim();
    const serverHost = (msg.serverHost || DEFAULT_CHAT_SERVER_HOST).toString().trim();
    const serverPort = Number.parseInt(msg.serverPort || DEFAULT_CHAT_SERVER_PORT, 10);

    if (!username) {
      sendJson(session.ws, { type: "error", message: "username es obligatorio" });
      return;
    }

    if (!serverHost || !Number.isInteger(serverPort) || serverPort <= 0 || serverPort > 65535) {
      sendJson(session.ws, { type: "error", message: "serverHost/serverPort invalidos" });
      return;
    }

    if (!session.tcp || session.tcp.destroyed) {
      try {
        await startTcpSession(session, serverHost, serverPort);
      } catch (error) {
        sendJson(session.ws, { type: "error", message: `No se pudo conectar al host: ${error.message}` });
        return;
      }
    }

    session.username = username;
    sendPacket(
      session,
      buildPacket({
        command: CMD.REGISTER,
        sender: username,
        payload: username
      })
    );
    return;
  }

  if (!session.username) {
    sendJson(session.ws, { type: "error", message: "Primero debes registrarte" });
    return;
  }

  try {
    switch (msg.type) {
      case "broadcast":
        sendPacket(
          session,
          buildPacket({
            command: CMD.BROADCAST,
            sender: session.username,
            payload: (msg.message || "").toString().slice(0, PAYLOAD_SIZE)
          })
        );
        break;
      case "direct":
        sendPacket(
          session,
          buildPacket({
            command: CMD.DIRECT,
            sender: session.username,
            target: (msg.target || "").toString().slice(0, TARGET_SIZE),
            payload: (msg.message || "").toString().slice(0, PAYLOAD_SIZE)
          })
        );
        break;
      case "list":
        sendPacket(session, buildPacket({ command: CMD.LIST, sender: session.username }));
        break;
      case "info":
        {
          const target = (msg.username || "").toString();
          session.pendingInfoTargets.push(target);
        sendPacket(
          session,
          buildPacket({
            command: CMD.INFO,
            sender: session.username,
            target: target.slice(0, TARGET_SIZE)
          })
        );
        }
        break;
      case "status":
        sendPacket(
          session,
          buildPacket({
            command: CMD.STATUS,
            sender: session.username,
            payload: (msg.status || "").toString().toUpperCase().slice(0, 15)
          })
        );
        break;
      case "logout":
        closeTcp(session);
        break;
      default:
        sendJson(session.ws, { type: "error", message: `Comando no soportado: ${msg.type}` });
        break;
    }
  } catch (error) {
    sendJson(session.ws, { type: "error", message: error.message });
  }
}

const wss = new WebSocketServer({ port: WS_PORT, path: WS_PATH });

wss.on("connection", (ws, request) => {
  const session = {
    ws,
    tcp: null,
    tcpBuffer: Buffer.alloc(0),
    username: "",
    pendingInfoTargets: []
  };

  const remote = request.socket.remoteAddress || "unknown";
  console.log(`[ws] connected ${remote}`);

  ws.on("message", (data) => {
    handleClientMessage(session, data);
  });

  ws.on("close", () => {
    closeTcp(session);
    console.log(`[ws] closed ${remote}`);
  });

  ws.on("error", (error) => {
    console.error(`[ws] error ${remote}:`, error.message);
    closeTcp(session);
  });
});

wss.on("listening", () => {
  console.log(`[gateway] ws://${DEFAULT_CHAT_SERVER_HOST}:${WS_PORT}${WS_PATH}`);
  console.log(`[gateway] default server ${DEFAULT_CHAT_SERVER_HOST}:${DEFAULT_CHAT_SERVER_PORT}`);
});

wss.on("error", (error) => {
  console.error("[gateway] websocket error:", error.message);
});
