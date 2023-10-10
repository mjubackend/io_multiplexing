#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#include <iostream>
#include <set>


using namespace std;


static const uint16_t PORT = 20101;


int main() {
  int serverSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

  // port 를 재사용할 수 있도록 옵션 설정
  int on = 1;
  if (setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
    cerr << "setsockopt() failed: " << strerror(errno) << endl;
    return 1;
  }

  // 특정 TCP port 에 bind
  struct sockaddr_in sin;
  memset(&sin, 0, sizeof(sin));
  sin.sin_family = AF_INET;
  sin.sin_addr.s_addr = INADDR_ANY;
  sin.sin_port = htons(PORT);
  if (bind(serverSock, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
    cerr << "bind() failed: " << strerror(errno) << endl;
    return 1;
  }

  // 서버 소켓을 passive socket 으로 변경. 이제 연결을 받을 수 있음.
  if (listen(serverSock, 10) < 0) {
    cerr << "listen() failed: " << strerror(errno) << endl;
    return 1;
  }

  // 현재 연결된 client sockets (= active sockets)
  set<int> clientSocks;

  while (true) {
    // I/O multiplexing 함수인 select() 호출을 위한 자료 구조(fd_set) 변수를 준비함
    // 우리는 읽기 이벤트만 처리할 예정이므로 fd_set rset 하나만 둠
    fd_set rset;
    FD_ZERO(&rset);  // 자료 구조 초기화

    FD_SET(serverSock, &rset);  // 해당 자료 구조에 서버 소켓을 등록함. 즉 서버 소켓의 읽기 이벤트 체크할 예정
    int maxFd = serverSock;  // select() 는 제일 큰 descriptor 번호가 필요하다.

    // 각 클라이언트 소켓 역시 읽기 이벤트 체크 대상으로 등록
    for (int clientSock : clientSocks) {
      FD_SET(clientSock, &rset);
      // 제일 큰 descriptor 번호를 얻어냄
      if (clientSock > maxFd) {
        maxFd = clientSock;
      }
    }

    // 주어진 descriptor 들에 대해서 읽기 이벤트를 기다림
    // 마지막 인자는 timeout 인데, NULL 을 넣으면 한정 없이 기다림
    // 만일 struct timeval 형태의 변수를 만들어서 timeout 시간을 0 으로 하면,
    // 기다리는 시간이 0, 즉 바로 확인만하고 반환함
    int numReady = select(maxFd + 1, &rset, NULL, NULL, NULL);
    if (numReady < 0) {
      cerr << "select() failed: " << strerror(errno) << endl;
      continue;
    } else if (numReady == 0) {
      // 어떤 소켓에서도 데이터가 들어오지 않고 있다.
      continue;
    }

    // 서버 소켓에서 읽기 이벤트가 발생했다면 새 클라이언트 접속이 있다는 것이다.
    if (FD_ISSET(serverSock, &rset)) {
      // 새로 들어오는 연결을 받음.
      // accept 는 이 연결을 위한 새로운 active socket 을 생성함
      memset(&sin, 0, sizeof(sin));
      unsigned int sin_len = sizeof(sin);
      int clientSock = accept(serverSock, (struct sockaddr *) &sin, &sin_len);
      if (clientSock < 0) {
        cerr << "accept() failed: " << strerror(errno) << endl;
      } else {
        clientSocks.insert(clientSock);
      }
    }

    // 각 클라이언트 소켓에 읽기 이벤트가 발생했다면
    //   1) 실제 읽을 데이터가 있거나
    //   2) 소켓이 닫힌 경우이다.
    set<int> willClose;
    for (int clientSock : clientSocks) {
      if (!FD_ISSET(clientSock, &rset)) {
        continue;
      }

      // 이 연결로부터 데이터를 읽음
      char buf[65536];
      int numRecv = recv(clientSock, buf, sizeof(buf), 0);
      if (numRecv == 0) {
        cout << "Socket closed: " << clientSock << endl;
        willClose.insert(clientSock);
      } else if (numRecv < 0) {
        cerr << "recv() failed: " << strerror(errno) << ", clientSock: " << clientSock << endl;
        willClose.insert(clientSock);
      } else {
        cout << "Received: " << numRecv << " bytes, clientSock: " << clientSock << endl;

        // 읽은 데이터를 그대로 돌려준다.
        // send() 는 요청한 byte 수만큼 전송을 보장하지 않으므로 반복해서 send 를 호출해야 될 수 있다.
        int offset = 0;
        while (offset < numRecv) {
          int numSend = send(clientSock, buf + offset, numRecv - offset, 0);
          if (numSend < 0) {
            cerr << "send() failed: " << strerror(errno) << ", clientSock: " << clientSock << endl;
            willClose.insert(clientSock);
            break;
          } else {
            cout << "Sent: " << numSend << " bytes, clientSock: " << clientSock << endl;
            offset += numSend;
          }
        }
      }
    }

    // 닫아야 되는 소켓을 정리한다.
    // 위의 for 문 안에서 처리하지 않는 이유는
    // clientSocks.erase() 가 for 를 도는 중간에 clientSocks 의 내용을 변경하기 때문이다.
    for (int clientSock : willClose) {
      close(clientSock);
      clientSocks.erase(clientSock);
    }
  }


  close(serverSock);
  return 0;
}
