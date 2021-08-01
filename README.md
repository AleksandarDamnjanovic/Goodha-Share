#Goodha Share

##Generals

This is command line tool, for file transferring over wifi or internet. It's built in C/++(c with minor c++ support). It is fast, easy to use and simple. Being less than 1k lines of code in size, makes it very easily to change according to your needs.

This tool has two binaries. One for receiver(server.bin) and one for sender(client.bin). When you start it, receiver runs continuously until you type `exit` in terminal. Client runs until file that you are sending is not sent completely and confirmation is received.

Godha Share is built to be used within wrapper for content transfer functionality for some other app, but it could be used as standalone application.

Communication port is set to be 44444.

##Arguments

###Receiver arguments

Receiver accepts two kind of arguments:
    
+ `-f` followed(without blank space) by path to the file that holds addresses you want to block or addresses you want to allow to reach to your mashine.
        
example:
            
`allow 192.168.0.25`

`allow 192.168.0.17`

`allow 192.168.0.3`
            
or
            

`block 192.168.0.25`

`block 192.168.0.17`

`block 192.168.0.3`

You can't use both allow and block keyword in the same instance of receiver. If you choose to use allow, it means that all other ip addresses except those listed are blocked. If you choose to use block keyword, it means that all other addresses except those listed are allowed.

+ `-r` followed(withoud blank space) with local path where you want files you have received to be stored.

Use arguments like ./server.bin -fmyFile.txt -r./ This means that address list is going to be load from myFile.txt stored in the same directory as server.bin and that same directory is going to be used as path for received files to be stored.

If you choose not to use arguments when you start receiver instance, everyone is going to be able to send files to your mashine, and local directory path is going to be used as storage place.

###Sender arguments

Arguments for sender are not arranged with keys. They have to be entered in particular order. All of arguments are mandatory.

1. IP address of receiving mashine.
2. Path to the file you want to send.
3. File name. Content on the target machine is going to appear under this name.
4. Relative path to this document on the target machine.

example:

`./client.bin 192.168.0.25 "/my mashine/my directory/my file.mp4" targetFile.mp4 /`

As you can see, I have set that file on the target machine will be named differently than on sender machine. At the end I have typed / that means that my file is going to be stored in the root document that is set as relative path on receiving machine. If I type /new/new/ as the last argument, for example, my file is going to be stored in "receiver relative path"/new/new directory. If such directory doesn't exist on the target mashine, receiver is going to create it.

##Runtime arguments

###Receiver arguments

+ `read` prints sender ip address, relative path, file name, transferred amount, and file size

+ `cancel` will ask you to type ip address of sender and stop transmission from that id

+ `ip add` will ask you to type in mode and address to update filter list

+ `ip remove` will ask you to type ip address to be removed from filter list

+ `path` is going to update curent receiver relative path

+ `filter` is going to list all filtered addresses and mode

+ `exit` will quit the application

###Sender arguments

+ `read` will print original file path, amound send and file size

+ `cancel` will break connection and exit the app

#How to compile?

By using makefile

`make build`

or by calling compiler yourself

`g++ -g server.cpp chunk.cpp constants.h -o server.bin -lpthread`

`g++ -g client.cpp chunk.cpp constants.h -o client.bin -lpthread`

