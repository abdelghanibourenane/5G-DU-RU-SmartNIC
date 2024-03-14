import struct
import numpy as np
import socket
import timeit
client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
N = 512

# Connect to the server; enter your server's IP and port here
client.connect(("192.168.1.10", 7))
def processing(n):
	timing =0
	for i in range(n):
		data_array = np.random.rand(N) + 1j * np.random.rand(N)


		# Define the maximum packet size
		max_packet_size = 1024

		# Pack the real and imaginary parts into bytes
		real_parts = [num.real for num in data_array]
		imaginary_parts = [num.imag for num in data_array]
		data_bytes = struct.pack('{}f'.format(len(data_array)), *real_parts) + struct.pack('{}f'.format(len(data_array)), *imaginary_parts)

		# Segmentize and send the data
		bytes_sent = 0
		start = timeit.default_timer()
		while bytes_sent < len(data_bytes):
			chunk = data_bytes[bytes_sent:bytes_sent + max_packet_size]
			client.send(chunk)
			bytes_sent += len(chunk)
			

		received_data = b""  # Initialize an empty bytes object to hold the received data

		# Keep receiving data until you have received at least N*8 bytes
		while len(received_data) < N * 8:
			chunk = client.recv(min(N * 8 - len(received_data), max_packet_size))  # Receive a chunk of data
			if not chunk:  # If no more data is received, break out of the loop
				break
			received_data += chunk  # Append the received chunk to the received_data buffer
		timing= timing+ (timeit.default_timer() - start)*1e6
	return timing/n
# Now, received_data contains at least N*8 bytes
# You can then unpack the received_data in chunks of 8 bytes
#complex_data = [struct.unpack('<ff', received_data[i:i+8]) for i in range(0, len(received_data), 8)]


print("The difference of time is :",processing(1000))

#print("The difference of time is :", timeit.default_timer() - start)
#print(len(received_data))
#complex_data = [complex(*struct.unpack('<ff', received_data[i:i+8])) for i in range(0, len(received_data), 8)]
#complex_data = [struct.unpack('<ff', received_data[i:i+8]) for i in range(0, len(received_data), 8)]




# Print the complex data



# Calculate the execution time


#complex_data = np.round(complex_data, decimals=1)
client.close()
#output_values = np.fft.fft(data_array)
#output_values = np.round(output_values, decimals=1)
"""
for i, value in enumerate(complex_data):
    print(f"Complex {i+1}: {value}")
for i, value in enumerate(output_values):
    print(f"Complex {i+1}: {value}")
"""

#print(np.array_equal(output_values,complex_data))
