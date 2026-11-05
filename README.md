# UDM_08_Chat_TCP_Client_Server
 
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17)
[![Qt](https://img.shields.io/badge/Qt-6-41CD52?logo=qt&logoColor=white)](https://www.qt.io/download-open-source)
![Qt](https://img.shields.io/badge/CMake-3.19+-064F8C?style=flat&logo=cmake&logoColor=white)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://choosealicense.com/licenses/mit/)
 
A client-server chat application over TCP, written in C++.

This project was developed for educational purposes as a course assignment.
 
## Installation
 
Download and install the following tools (CMake and Qt Creator are included in the Qt installer):
 
- Qt 6: https://www.qt.io/download-open-source

- C++17 compiler VS Code: https://code.visualstudio.com/Download

- Git: https://git-scm.com/downloads (Download the source code as a ZIP file from GitHub, or clone it with Git)

Then clone the repository, open `CMakeLists.txt` in Qt Creator, select a Qt 6 kit and build.
 
```bash
git clone https://github.com/thanhhung4412y/UDM_08_Chat_TCP_Client_Server.git
```
 
> [!IMPORTANT]
> `CMakeLists.txt` contains `add_subdirectory(untitled)`, so the `untitled/` directory must exist. If you don't use it, remove that line.
 
## Usage
 
Run the server (`chattcp1`) first, then one or more clients (`ChatClient`). The server listens on port `12345`.
 
Features: online user list, private messages, reply, forward, emoji, stickers and image sharing(still working on video sharing).
 
## Protocol
 
Each packet is one JSON object on a single line, ending with a newline character (UTF-8).
 
Example:
 
```json
{"type":"private","to":"Hung","text":"hello"}
```
 
### Client to Server
 
- `join` (name): log in with a user name
- `chat` (text): send a message to the public room
- `private` (to, text): send a private message to one user
- `reply` (text, quote): reply to a quoted message
- `forward` (to, text): forward a message; empty `to` means the public room
- `sticker` (to, id): send a sticker
- `image` (to, data): send an image; data is a Base64-encoded JPEG
### Server to Client
 
- `joined` (name): login successful
- `system` (text): system notice, e.g. someone joined or left
- `users` (names): list of online users
- `chat` (sender, text): public room message
- `private` (sender, to, text): private message
- `reply` (sender, text, quote): reply message
- `forward` (sender, to, text): forwarded message
- `sticker` (sender, to, id): sticker
- `image` (sender, to, data): image
- `error` (text): error message

## Limitations
 
- No accounts, messages are not stored.
- No offline user list and no message history synchronization (online users only).
 
## License
 
[MIT](https://choosealicense.com/licenses/mit/)
