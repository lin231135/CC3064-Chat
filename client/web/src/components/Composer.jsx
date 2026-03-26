import { useState } from "react";

function Composer({ chatLabel, onSend, disabled }) {
  const [text, setText] = useState("");

  const handleSubmit = (event) => {
    event.preventDefault();
    const value = text.trim();
    if (!value) {
      return;
    }
    onSend(value);
    setText("");
  };

  const placeholder = chatLabel === "General" ? "Mensaje para todos" : `Mensaje privado para ${chatLabel}`;

  return (
    <form className="composer panel" onSubmit={handleSubmit}>
      <input
        type="text"
        value={text}
        onChange={(event) => setText(event.target.value)}
        placeholder={placeholder}
        disabled={disabled}
        maxLength={957}
      />
      <button type="submit" disabled={disabled || !text.trim()}>
        Enviar
      </button>
    </form>
  );
}

export default Composer;
