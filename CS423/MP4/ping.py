#!/usr/bin/env python
 
import os, sys, socket, struct, select
from timeit import default_timer as timer
# From /usr/include/linux/icmp.h; your milage may vary.
ICMP_ECHO_REQUEST = 8 # Seems to be the same on Solaris.
 
 
def checksum(source_string):
    sum = 0
    countTo = (len(source_string)/2)*2
    count = 0
    while count<countTo:
        thisVal = ord(source_string[count + 1])*256 + ord(source_string[count])
        sum = sum + thisVal
        sum = sum & 0xffffffff # Necessary?
        count = count + 2
 
    if countTo<len(source_string):
        sum = sum + ord(source_string[len(source_string) - 1])
        sum = sum & 0xffffffff # Necessary?
 
    sum = (sum >> 16)  +  (sum & 0xffff)
    sum = sum + (sum >> 16)
    answer = ~sum
    answer = answer & 0xffff
 
    # Swap bytes. Bugger me if I know why.
    answer = answer >> 8 | (answer << 8 & 0xff00)
 
    return answer
 
 
def receive_one_ping(my_socket, ID, timeout):
    """
    receive the ping from the socket.
    """
    timeLeft = timeout
    while True:
        startedSelect = timer()
        whatReady = select.select([my_socket], [], [], timeLeft)
        howLongInSelect = (timer() - startedSelect)
        if whatReady[0] == []: # Timeout
            return
 
        timeReceived = timer()
        recPacket, addr = my_socket.recvfrom(1024)
        icmpHeader = recPacket[20:28]
        type, code, checksum, packetID, sequence = struct.unpack(
            "bbHHh", icmpHeader
        )
        if packetID == ID:
            bytesInDouble = struct.calcsize("d")
            timeSent = struct.unpack("d", recPacket[28:28 + bytesInDouble])[0]
            return timeReceived - timeSent
 
        timeLeft = timeLeft - howLongInSelect
        if timeLeft <= 0:
            return
 
 
def send_one_ping(my_socket, dest_addr, ID, datasize):
    """
    Send one ping to the given >dest_addr<.
    """
    dest_addr  =  socket.gethostbyname(dest_addr)
 
    # Header is type (8), code (8), checksum (16), id (16), sequence (16)
    my_checksum = 0
 
    # Make a dummy heder with a 0 checksum.
    header = struct.pack("bbHHh", ICMP_ECHO_REQUEST, 0, my_checksum, ID, 1)
    bytesInDouble = struct.calcsize("d")
    data = (datasize - bytesInDouble) * "Q"
    data = struct.pack("d", timer()) + data
 
    # Calculate the checksum on the data and the dummy header.
    my_checksum = checksum(header + data)
 
    # Now that we have the right checksum, we put that in. It's just easier
    # to make up a new header than to stuff it into the dummy.
    header = struct.pack(
        "bbHHh", ICMP_ECHO_REQUEST, 0, socket.htons(my_checksum), ID, 1
    )
    packet = header + data
    print 'sizeofdata=' + str(len(data))
    my_socket.sendto(packet, (dest_addr, 1)) # Don't know about the 1


def my_ping(dest_addr, datasize, timeout):
    """
    Returns either the delay (in seconds) or none on timeout.
    """
    icmp = socket.getprotobyname("icmp")
    try:
        my_socket = socket.socket(socket.AF_INET, socket.SOCK_RAW, icmp)
    except socket.error, (errno, msg):
        if errno == 1:
            # Operation not permitted
            msg = msg + (
                " - Note that ICMP messages can only be sent from processes"
                " running as root."
            )
            raise socket.error(msg)
        raise # raise the original error

    my_ID = os.getpid() & 0xFFFF

    send_one_ping(my_socket, dest_addr, my_ID, datasize)
    delay = receive_one_ping(my_socket, my_ID, timeout)

    my_socket.close()
    return delay
 
def verbose_ping(dest_addr, option, timeout = 10, count = 1):
    if option == 'delay':
        datasize = 1
    elif option == 'bandwidth':
        datasize =  50 * 1024 # 50 KB
    for i in xrange(count):
        print "ping %s..." % dest_addr,
        try:
            delay = my_ping(dest_addr, datasize, timeout)
        except socket.gaierror, e:
            print "failed. (socket error: '%s')" % e[1]
            return False

        if delay  ==  None:
            print "failed. (timeout within %ssec.)" % timeout
            return False
        else:
            print "get verbose ping in %0.4f ms" % (1000*delay)
            if option == 'delay':
                print 'delay=%.4f ms\n' % (1000/2*delay)
                return 1000/2*delay
            elif option == 'bandwidth':
                print 'bandwidth=%.2f KB/s\n' % (50.0*2/delay)
                return 1.0*2/delay
            else:
                return False 

if __name__ == '__main__':
    #verbose_ping("google.com", 'delay')
    #verbose_ping("google.com", 'bandwidth')
    verbose_ping("172.22.156.19", 'delay')
    verbose_ping("172.22.156.19", 'bandwidth')

