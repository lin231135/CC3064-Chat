function Sidebar({
  username,
  selfIp,
  status,
  users,
  activeChat,
  onChatSelect,
  onStatusChange,
  onRefreshUsers,
  connectionState
}) {
  return (
    <aside className="sidebar panel">
      <div className="identity">
        <p className="eyebrow">Conectado como</p>
        <h2>
          {username}
          <small className="identity-ip">{selfIp || "IP N/A"}</small>
        </h2>
        <div className="identity-pills">
          <span className={`connection-pill is-${connectionState}`}>{connectionState}</span>
          <span className={`status-pill status-${status.toLowerCase()}`}>Estado: {status}</span>
        </div>
      </div>

      <div className="field-block">
        <label htmlFor="status">Estado</label>
        <select id="status" value={status} onChange={(event) => onStatusChange(event.target.value)}>
          <option value="ACTIVE">ACTIVE</option>
          <option value="BUSY">BUSY</option>
          <option value="INACTIVE">INACTIVE</option>
        </select>
      </div>

      <div className="field-block">
        <div className="row space-between">
          <label>Chats</label>
          <button type="button" className="ghost-btn" onClick={onRefreshUsers}>
            Actualizar
          </button>
        </div>
        <div className="chat-list">
          <button
            type="button"
            className={`chat-item ${activeChat.type === "general" ? "active" : ""}`}
            onClick={() => onChatSelect({ type: "general", user: "" })}
          >
            <span>General</span>
            <small>Todos los usuarios</small>
          </button>

          {users.map((user) => (
            <button
              type="button"
              key={user.username}
              className={`chat-item ${activeChat.type === "private" && activeChat.user === user.username ? "active" : ""}`}
              onClick={() => onChatSelect({ type: "private", user: user.username })}
            >
              <span>{user.username}</span>
                <small className="chat-item-meta">
                  <span className={`member-status member-status-${(user.status || "ACTIVE").toLowerCase()}`}>
                    {user.status || "ACTIVE"}
                  </span>
                  <span>{user.ip || "IP N/A"}</span>
                </small>
            </button>
          ))}
        </div>
      </div>
    </aside>
  );
}

export default Sidebar;
