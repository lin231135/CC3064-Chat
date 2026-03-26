function MessageList({ messages, username }) {
  if (!messages.length) {
    return (
      <div className="messages-empty panel">
        <p>Aun no hay mensajes. Escribe el primero para comenzar.</p>
      </div>
    );
  }

  return (
    <div className="messages panel" role="log" aria-live="polite">
      {messages.map((msg) => {
        const own = msg.from === username;
        const isSystem = (msg.from || "").toUpperCase() === "SERVER" || (msg.scope || "").toLowerCase() === "system";
        const bubbleClass = isSystem ? "bubble-system" : own ? "bubble-own" : "bubble-other";
        return (
          <article key={msg.id} className={`bubble ${bubbleClass}`}>
            <header>
              <strong>{msg.from || "SERVER"}</strong>
              <small>{msg.scope || "system"}</small>
            </header>
            <p>{msg.text}</p>
            <time>{msg.timestamp}</time>
          </article>
        );
      })}
    </div>
  );
}

export default MessageList;
