If you run xsane as root xsane comes up with a warning message at startup.
Some people asked me to remove this warning, e.g:

Q: "... But there is something I very disagree with: This horrible message when
   I launch XSane, saying me that I mustn't run XSane as root.
   If I run as root it's MY problem! Running as root has inconvenience, but
   has also lots of advantages. Like I am the only user of my conputer I
   run as root"

A: 1) It does not matter if you are the only user on your system. There still
      is a big security problem also in this case. When you run XSane as root
      then XSane has pemission to remove or change any file on your system.
      XSane is a really complex program and for sure there are still bugs
      that may cause an unexpected behaviour like removing or writing into
      files. Imagine what happens when XSane removes your home directory or
      any important system files. Another issue is that you can accidently
      remove or change all files on the system using XSane.

   2) early versions of XSane did not print this message and a lot of people
      did run XSane as root. This caused a lot of problems and I got a lot of
      problem reports and please-help-me mails. This took a lot of my time.
      So it also is my problem when several people run XSane as root.


   Please think about your decision to do all you work as root. This really
   is dangerous. I do not know any professional system adminstrator who works
   all the time as root. All system administrators work as a normal user
   and if there is something that has to be done as root, then the admin
   gets root permission only for this command.

   Please beleve me that these people do know a lot of their machines and that
   is the reason why they know it is dangerous to work all the time as root.

   Doing a "rm -f *" in the wrong directory can kill your complete system when
   you run as root. As normal user nothing will happen with a bit luck.

   The decision if you work as root all the time has nothing to do if you are
   the only user on the system.


Q: "But I am using a parallel port scanner and need to be root to access the scanner"

A: It is not necessary that the frontend (xsane) runs as root. The backend (driver) that
   does access your scanner may need root access. If possible you sould compile your
   backend with a parallel port library that allows acces as non privileged user.
   If your backend does not support such a library then you can set up network scanning
   on your system. Configure saned to run as root. To connect to the scanner connect
   via the network protocol to localhost, e.g.: xsane net:localhost:epson
   
