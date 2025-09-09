import usb.core
import time
import math

strip_length = 22 # LED-Streifen Länge in LEDs

# USB-Gerät Initialisieren
dev = usb.core.find(idVendor=0xcafe, idProduct=0x1234)
if dev is None:
    raise ValueError("USB-Gerät nicht gefunden.")

usb.util.claim_interface(dev, 0)

# Create Init Data packet
data_packet_1 = b'\x01' # \x01 = Init
data_packet_1 += (strip_length >> 8).to_bytes(1, byteorder='big')
data_packet_1 += (strip_length & 0xFF).to_bytes(1, byteorder='big')

# Fill Init Data packet to 64 byte
for i in range(61):
    data_packet_1 += b'\x00'

# Send Init Data packet
dev.write(0x02, data_packet_1)
#data_packet_1_received = dev.read(0x81, 64) # Read USB-ACK
#if(data_packet_1 == bytes(data_packet_1_received)):
#    print("Successfully sent Length of " + str(strip_length) + " to USB-Device.\n")

colors = [b'\x33\x00\x00', b'\x00\x33\x00', b'\x00\x00\x33'] # LED Values (20% brightness, 100% is too bright for development)

# Create LED Data packet (64 bytes, 1 ctrl byte + 63 RGB bytes, 21 LEDs)
data_packet_2 = b'\x00' # \x00 = LED Data

for j in range(7):
    data_packet_2 += colors[0]
    data_packet_2 += colors[1]
    data_packet_2 += colors[2]


# Send LED Data packet 3 times (21 LEDs * 3 = 63 LEDs > 50 LEDs)
for i in range(math.ceil(strip_length / 21)):
    dev.write(0x02, data_packet_2)
    print(i)
    #data_packet_2_received = dev.read(0x81, 64) # Read USB-ACK
    #if(data_packet_2 == bytes(data_packet_2_received)):
     #   print("Successfully sent " + str(i + 1) + ". LED-Data-Packet to USB-Device.")

print("")
# Request length and print it
dev.write(0x02, b'\x02')

length = dev.read(0x81, 64) # Read USB-Length
max_length = (length[3] << 8) | length[4]
length = (length[1] << 8) | length[2]

print("Requesting length from USB-Device: " + str(length) + "\n")
print("Requesting max_length from USB-Device: " + str(max_length) + "\n")
print("Requesting buffer from USB-Device:")
# Request buffer and print it
for i in range(0, math.ceil(length / 21)):
    # Create Init Data packet
    request_data_pkg = b'\x03' # \x01 = Init
    request_data_pkg += (i >> 8).to_bytes(1, byteorder='big')
    request_data_pkg += (i & 0xFF).to_bytes(1, byteorder='big')

    # Fill Init Data packet to 64 byte
    for j in range(61):
        request_data_pkg += b'\x00'
    #print(request_data_pkg)
    dev.write(0x02, request_data_pkg)
    buffer = dev.read(0x81, 64)# Read USB-Buffer
    print(buffer)

# Clear LEDs
data_packet_3 = b'\x99'
# Fill Clear LED packet to 64 byte
for i in range(63):
    data_packet_1 += b'\x00'
dev.write(0x02, data_packet_3)


# USB-Verbindung schließen
usb.util.dispose_resources(dev)