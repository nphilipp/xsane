Installation of XSane WIN32:

The archive contains a directory "sane". You must
extract the zip archive to c:\ so that you get
the directory c:\sane

XSane will not work when you put it to any other place!


Configuration of XSane WIN32:

edit c:\sane\etc\sane.d\net.conf
and put the IP address of the server where the scanner
is connected into one line, e.g:
192.168.0.1

make sure that c:\sane\etc\sane.d\dll.conf
contains the line
net


Start XSane WIN32:

Execute c:\sane\bin\xsane.exe
