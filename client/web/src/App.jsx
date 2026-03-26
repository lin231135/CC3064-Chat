import { useEffect, useMemo, useRef, useState } from "react";
import Sidebar from "./components/Sidebar";
import MessageList from "./components/MessageList";
import Composer from "./components/Composer";
import { GatewayClient } from "./lib/gatewayClient";
import { SERVER_EVENTS } from "./lib/chatProtocol";

const defaultWsUrl = import.meta.env.VITE_WS_URL || "ws://localhost:8080/ws";
const defaultServerHost = import.meta.env.VITE_CHAT_SERVER_HOST || "127.0.0.1";
const defaultServerPort = import.meta.env.VITE_CHAT_SERVER_PORT || "5000";

function parseGateway(url) {
  try {
    const parsed = new URL(url);
    return {
      host: parsed.hostname || "localhost",
      port: parsed.port || "8080",
      path: parsed.pathname || "/ws"
    };
  } catch {
    return {
      host: "localhost",
      port: "8080",
      path: "/ws"
    };
  }
}

function buildGatewayUrl(host, port, path) {
  const protocol = window.location.protocol === "https:" ? "wss" : "ws";
  const cleanPath = path.startsWith("/") ? path : `/${path}`;
  return `${protocol}://${host}:${port}${cleanPath}`;
}

function nowTime() {
  return new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });
}

function isDuplicateIpError(message) {
  return (message || "").toLowerCase().includes("ip ya conectada");
}

function App() {
  const gateway = parseGateway(defaultWsUrl);
  const [serverIp, setServerIp] = useState(defaultServerHost);
  const [serverPort, setServerPort] = useState(defaultServerPort);
  const [usernameInput, setUsernameInput] = useState("");

  const [username, setUsername] = useState("");
  const [connected, setConnected] = useState(false);
  const [connectionState, setConnectionState] = useState("offline");
  const [status, setStatus] = useState("ACTIVE");
  const [selfIp, setSelfIp] = useState("");
  const [activeChat, setActiveChat] = useState({ type: "general", user: "" });
  const [users, setUsers] = useState([]);
  const [messages, setMessages] = useState([]);
  const [notice, setNotice] = useState("Conecta para comenzar el chat");

  const gatewayRef = useRef(null);
  const lastOwnBroadcastRef = useRef({ text: "", at: 0 });

  const selfName = username || usernameInput.trim();
  const otherUsers = useMemo(() => users.filter((user) => user.username !== selfName), [users, selfName]);
  const activeChatLabel = activeChat.type === "general" ? "General" : activeChat.user;
  const systemMessages = useMemo(
    () => messages.filter((msg) => (msg.scope || "").toLowerCase() === "system"),
    [messages]
  );
  const visibleMessages = useMemo(() => {
    if (activeChat.type === "general") {
      return messages.filter((msg) => msg.scope === "broadcast");
    }

    return messages.filter((msg) => {
      if (msg.scope !== "direct") {
        return false;
      }
      const peer = msg.peer || (msg.from === selfName ? msg.target : msg.from);
      return peer === activeChat.user;
    });
  }, [activeChat, messages, selfName]);
  const computedGatewayUrl = useMemo(() => buildGatewayUrl(gateway.host, gateway.port, gateway.path), [gateway.host, gateway.port, gateway.path]);

  useEffect(() => {
    return () => {
      if (gatewayRef.current) {
        gatewayRef.current.close();
      }
    };
  }, []);

  const appendMessage = (message) => {
    setMessages((prev) => [...prev, { id: crypto.randomUUID(), timestamp: nowTime(), ...message }]);
  };

  const wireGateway = (gateway) => {
    gateway.on("open", () => {
      setConnectionState("connecting");
      setConnected(false);
      setNotice("Conectado al gateway, validando registro...");
      gateway.register(usernameInput.trim(), serverIp.trim(), Number(serverPort));
    });

    gateway.on("close", () => {
      setConnectionState("offline");
      setConnected(false);
      setNotice("Conexion cerrada. Puedes reintentar.");
    });

    gateway.on("error", (event) => {
      setNotice(event.message || "Error de conexion");
    });

    gateway.on("message", (event) => {
      switch (event.type) {
        case SERVER_EVENTS.CONNECTED:
          setUsername(event.username || usernameInput.trim());
          setStatus(event.status || "ACTIVE");
          setConnectionState("online");
          setConnected(true);
          setNotice("Conexion establecida");
          gateway.requestUsers();
          if (event.message) {
            appendMessage({ from: "SERVER", scope: "system", text: event.message });
          }
          break;
        case SERVER_EVENTS.MESSAGE:
          {
            const from = event.from || "SERVER";
            const target = event.target || "ALL";
            const scope = event.scope || "broadcast";

            if (scope === "system" && from === "SERVER") {
              const textUpper = (event.text || "").toUpperCase();
              if (textUpper.includes("INACTIVE")) {
                setStatus("INACTIVE");
              } else if (textUpper.includes("ACTIVE") && !textUpper.includes("INACTIVE")) {
                setStatus("ACTIVE");
              }
            }

            // If backend/gateway echoes own broadcast, avoid duplicate in General chat.
            if (scope === "broadcast" && from === selfName) {
              break;
            }

            const peer = scope === "direct" ? (from === selfName ? target : from) : "general";
            appendMessage({
              from,
              target,
              peer,
              scope,
              text: event.text || ""
            });
          }
          break;
        case SERVER_EVENTS.USER_LIST:
          {
            const incoming = Array.isArray(event.users) ? event.users : [];
            setUsers((prev) => {
              const prevByName = new Map(prev.map((user) => [user.username, user]));
              return incoming.map((user) => ({
                ...user,
                ip: prevByName.get(user.username)?.ip || ""
              }));
            });

            incoming
              .filter((user) => user.username)
              .forEach((user) => {
                try {
                  gateway.requestUserInfo(user.username);
                } catch {
                  // Ignore transient request errors while list updates.
                }
              });
          }
          break;
        case SERVER_EVENTS.USER_INFO:
          if (event.user) {
            if (event.user.username === selfName) {
              setSelfIp(event.user.ip || "");
              if (event.user.status) {
                setStatus(event.user.status);
              }
            }
            setUsers((prev) =>
              prev.map((user) =>
                user.username === event.user.username
                  ? {
                      ...user,
                      ip: event.user.ip || "",
                      status: event.user.status || user.status || "ACTIVE"
                    }
                  : user
              )
            );
          }
          break;
        case SERVER_EVENTS.STATUS_CHANGED:
          setStatus(event.status || "ACTIVE");
          break;
        case SERVER_EVENTS.ERROR:
          if (isDuplicateIpError(event.message)) {
            setConnected(false);
            setConnectionState("offline");
            setUsers([]);
            setMessages([]);
            setNotice("No se puede ingresar: ya hay una conexion activa desde esta IP.");
            if (gatewayRef.current) {
              gatewayRef.current.close();
              gatewayRef.current = null;
            }
          } else {
            setNotice(event.message || "Error enviado por gateway");
          }
          break;
        default:
          break;
      }
    });
  };

  const handleConnect = async (event) => {
    event.preventDefault();
    const cleanUser = usernameInput.trim();
    const cleanIp = serverIp.trim();
    const cleanPort = serverPort.trim();

    if (!cleanUser || !cleanIp || !cleanPort) {
      setNotice("Debes ingresar usuario, IP y puerto");
      return;
    }

    setConnectionState("connecting");
    setNotice("Conectando...");

    try {
      if (gatewayRef.current) {
        gatewayRef.current.close();
      }
      const gateway = new GatewayClient(computedGatewayUrl);
      gatewayRef.current = gateway;
      wireGateway(gateway);
      await gateway.connect();
    } catch (error) {
      setConnectionState("offline");
      setConnected(false);
      setNotice(`No se pudo conectar: ${error.message}`);
    }
  };

  const handleSend = (text) => {
    if (!gatewayRef.current) {
      return;
    }

    try {
      if (activeChat.type === "private") {
        if (!activeChat.user) {
          setNotice("Selecciona un chat privado para enviar mensaje directo");
          return;
        }
        gatewayRef.current.direct(activeChat.user, text);
        appendMessage({
          from: selfName,
          target: activeChat.user,
          peer: activeChat.user,
          scope: "direct",
          text
        });
      } else {
        const now = Date.now();
        if (lastOwnBroadcastRef.current.text === text && now - lastOwnBroadcastRef.current.at < 700) {
          return;
        }

        gatewayRef.current.broadcast(text);
        lastOwnBroadcastRef.current = { text, at: now };
        appendMessage({ from: selfName, target: "ALL", peer: "general", scope: "broadcast", text });
      }
    } catch (error) {
      setNotice(error.message);
    }
  };

  const handleStatusChange = (nextStatus) => {
    setStatus(nextStatus);
    if (gatewayRef.current && connected) {
      try {
        gatewayRef.current.setStatus(nextStatus);
      } catch (error) {
        setNotice(error.message);
      }
    }
  };

  const refreshUsers = () => {
    if (!gatewayRef.current || !connected) {
      return;
    }

    try {
      gatewayRef.current.requestUsers();
      setNotice("Lista de usuarios actualizada");
    } catch (error) {
      setNotice(error.message);
    }
  };

  const disconnect = () => {
    if (gatewayRef.current) {
      try {
        gatewayRef.current.logout();
      } catch {
        // ignore logout failures before close
      }
      gatewayRef.current.close();
      gatewayRef.current = null;
    }
    setConnected(false);
    setConnectionState("offline");
    setUsername("");
    setSelfIp("");
    setActiveChat({ type: "general", user: "" });
  };

  return (
    <main className="app-shell">
      <header className="app-header panel">
        <div>
          <p className="eyebrow">Sistemas Operativos</p>
          <h1>Cliente Web de Chat</h1>
        </div>
        <div className="header-actions">
          <span className={`connection-pill is-${connectionState}`}>{connectionState}</span>
          {connected ? (
            <button type="button" className="ghost-btn" onClick={disconnect}>
              Salir
            </button>
          ) : null}
        </div>
      </header>

      {!connected ? (
        <section className="login-card panel">
          <h2>Conectar cliente</h2>
          <form onSubmit={handleConnect}>
            <label htmlFor="host">IP del host (servidor C)</label>
            <input
              id="host"
              value={serverIp}
              onChange={(event) => setServerIp(event.target.value)}
              placeholder="127.0.0.1"
            />

            <label htmlFor="port">Puerto del host</label>
            <input
              id="port"
              value={serverPort}
              onChange={(event) => setServerPort(event.target.value)}
              placeholder="5000"
            />

            <label htmlFor="username">Usuario</label>
            <input
              id="username"
              value={usernameInput}
              onChange={(event) => setUsernameInput(event.target.value)}
              placeholder="ej. alice"
              maxLength={31}
            />

            <button type="submit" className="primary-btn" disabled={connectionState === "connecting"}>
              {connectionState === "connecting" ? "Conectando..." : "Entrar"}
            </button>
          </form>
          <p className="notice">Gateway WebSocket: {computedGatewayUrl}</p>
          <p className="notice">{notice}</p>
        </section>
      ) : (
        <section className="chat-grid">
          <Sidebar
            username={selfName}
            selfIp={selfIp}
            status={status}
            users={otherUsers}
            activeChat={activeChat}
            onChatSelect={setActiveChat}
            onStatusChange={handleStatusChange}
            onRefreshUsers={refreshUsers}
            connectionState={connectionState}
          />

          <section className="chat-main">
            <div className="chat-meta panel">
              <strong>{activeChatLabel} · {visibleMessages.length} mensajes</strong>
              <button type="button" className="ghost-btn" onClick={() => setMessages([])}>
                Limpiar
              </button>
            </div>
            <MessageList messages={visibleMessages} username={selfName} />
            <Composer
              chatLabel={activeChatLabel}
              onSend={handleSend}
              disabled={false}
            />

            <section className="system-feed panel" aria-live="polite">
              <div className="system-feed-header">
                <strong>Sistema</strong>
                <small>{systemMessages.length} eventos</small>
              </div>

              <div className="system-feed-list">
                {systemMessages.length === 0 ? (
                  <p className="system-empty">Sin eventos del sistema</p>
                ) : (
                  systemMessages
                    .slice(-8)
                    .reverse()
                    .map((msg) => (
                      <article key={msg.id} className="system-item">
                        <p>{msg.text}</p>
                        <time>{msg.timestamp}</time>
                      </article>
                    ))
                )}
              </div>
            </section>
          </section>
        </section>
      )}

      <footer className="app-footer">
        <p>{notice}</p>
      </footer>
    </main>
  );
}

export default App;
