export const CLIENT_COMMANDS = {
  REGISTER: "register",
  BROADCAST: "broadcast",
  DIRECT: "direct",
  LIST: "list",
  INFO: "info",
  STATUS: "status",
  LOGOUT: "logout"
};

export const SERVER_EVENTS = {
  CONNECTED: "connected",
  MESSAGE: "message",
  USER_LIST: "user_list",
  USER_INFO: "user_info",
  STATUS_CHANGED: "status_changed",
  ERROR: "error"
};

export const USER_STATUS = ["ACTIVE", "BUSY", "INACTIVE"];
