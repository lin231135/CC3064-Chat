import { CLIENT_COMMANDS } from "./chatProtocol";

export class GatewayClient {
  constructor(url) {
    this.url = url;
    this.ws = null;
    this.listeners = new Map();
  }

  on(eventName, callback) {
    const existing = this.listeners.get(eventName) || [];
    this.listeners.set(eventName, [...existing, callback]);
    return () => {
      const current = this.listeners.get(eventName) || [];
      this.listeners.set(
        eventName,
        current.filter((fn) => fn !== callback)
      );
    };
  }

  emit(eventName, payload) {
    const handlers = this.listeners.get(eventName) || [];
    handlers.forEach((handler) => handler(payload));
  }

  connect() {
    return new Promise((resolve, reject) => {
      try {
        this.ws = new WebSocket(this.url);
      } catch (error) {
        reject(error);
        return;
      }

      this.ws.onopen = () => {
        this.emit("open");
        resolve();
      };

      this.ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          this.emit("message", data);
        } catch (error) {
          this.emit("error", { message: "Mensaje invalido del gateway", detail: error.message });
        }
      };

      this.ws.onerror = () => {
        this.emit("error", { message: "Error de red en WebSocket" });
      };

      this.ws.onclose = (event) => {
        this.emit("close", {
          code: event.code,
          reason: event.reason
        });
      };
    });
  }

  send(payload) {
    if (!this.ws || this.ws.readyState !== WebSocket.OPEN) {
      throw new Error("WebSocket no esta conectado");
    }
    this.ws.send(JSON.stringify(payload));
  }

  register(username, serverHost, serverPort) {
    this.send({
      type: CLIENT_COMMANDS.REGISTER,
      username,
      serverHost,
      serverPort
    });
  }

  broadcast(message) {
    this.send({ type: CLIENT_COMMANDS.BROADCAST, message });
  }

  direct(target, message) {
    this.send({ type: CLIENT_COMMANDS.DIRECT, target, message });
  }

  requestUsers() {
    this.send({ type: CLIENT_COMMANDS.LIST });
  }

  requestUserInfo(username) {
    this.send({ type: CLIENT_COMMANDS.INFO, username });
  }

  setStatus(status) {
    this.send({ type: CLIENT_COMMANDS.STATUS, status });
  }

  logout() {
    this.send({ type: CLIENT_COMMANDS.LOGOUT });
  }

  close() {
    if (this.ws && this.ws.readyState < WebSocket.CLOSING) {
      this.ws.close(1000, "client-exit");
    }
  }
}
