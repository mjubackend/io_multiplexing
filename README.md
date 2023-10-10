# 기초적인 IO Multiplexing 에 대한 예제 코드

## tcp_server2.cpp

select() 를 이용해서 여러 클라이언트를 서비스하는 TCP server

## tcp_server3.cpp

3초간 새로운 데이터가 없는 클라이언트들을 Idle timeout 시키는 TCP server

## lab5_test10_with_nonblocking.cpp

lab5 의 test10.cpp (TCP server) 를 non-blocking socket 으로 변경함.
lab5 의 test7.cpp 의 send() 앞에 sleep(3) 같은 코드를 두면 동작을 확인 할 수 있음.
