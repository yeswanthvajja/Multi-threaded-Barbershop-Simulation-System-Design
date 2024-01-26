Compile the Spinlock.cpp, Server.c and client.c file:

1) g++ -c -o spinlock.o spinlock.cpp - This creates an object file spinlock.o for the spinlock implementation.

2) gcc -c -o server.o server.c - This produces an object file server.o for the server.

3) gcc -o server server.o spinlock.o -lpthread -lstdc++ - This generates the executable "server".

4) gcc -o client client.c - his creates the executable "client".

Running the Simulation:

1) ./server - The server will start and wait for client connections.

2) ./client - The client will connect to the server and simulate a customer's visit to the barbershop.

Expected Output:

Server Side:

1) The server prints logs of barbershop activities, such as customers arriving, being seated, receiving haircuts, and making payments.
2) Status updates of barbers and customers are displayed, including waiting times and service completions.
3) The server also tracks and displays statistics like total customers served and average waiting times.

Client Side:

1) The client displays messages received from the server, such as notifications about waiting, haircut start, and payment processing.
2) If the barbershop is full, the client will display a message indicating that there is no space available.