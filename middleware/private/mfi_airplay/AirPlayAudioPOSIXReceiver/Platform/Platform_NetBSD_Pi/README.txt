NetBSD AirPlay Platform Implementation for Raspberry Pi
-------------------------------------------------------

This directory contains the AirPlay Platform implementation for NetBSD on Raspberry Pi. This has been tested and works on Raspberry
Pi Model B and Model B+. 

Developer requires the following to make use of this platform implementation:

1. Raspberry Pi Model B or Model B+
    Refer to http://www.raspberrypi.org/ for more details.

2. SD Card with NetBSD Pi image
    - Current NetBSD SD Card image for Raspberry Pi is available at:
        ftp://ftp.netbsd.org/pub/NetBSD/misc/jun/raspberry-pi/2014-08-01-earmhf/2014-08-01-netbsd-raspi.img.gz

    - Subscribe to mailing list  port-arm@netbsd.org to stay upto date with new NetBSD Pi image updates.
        
3. Apple Authentication Coprocessor 
    Connect Authentication Coprocessor's SDA/SCL/GND pins to Raspberry Pi's SDA/SCL/GND pins which is part of Raspberry Pi's GPIO
    pin rows.

4. NetBSD development toolchain




Outstanding NetBSD Pi issues
----------------------------

The following oustanding issues are known with the NetBSD Pi Port:

1. Audio Driver issues on Pi running NetBSD (2014-05-10) image - hangs audio applications like audioplay (NetBSD Problem Report #48805)
    - http://gnats.netbsd.org/48805
    - Leads to airplay audio to stop and further audio applications to hang

2. Raspberry Pi Audio volume control does not work (NetBSD Problem Report #49057)
    - http://gnats.netbsd.org/49057
    - Leads to no volume change support. Audio always plays at the same volume.

3. Raspberry Pi i2c  ioctl returns success even though i2c slave device NACKed (NetBSD Problem Report #48932)
    - http://gnats.netbsd.org/48932
    - Some kludges required in MFi platform implementation to work around these bugs.
