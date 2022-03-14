import string
import itertools

def hash_offset(inp):
	hash_val = 19780211
	bucket_number = 0x1ffff
	for i in inp:
		hash_val = ((hash_val * 37) + ord(i)) & 0xffffffffffffffff
	return_value = ((hash_val % bucket_number) * 0x4) + 0x100
	return return_value

base_array = []

seen_hashes = {}

# Make the lsit to iterate through
for i in string.ascii_letters:
	base_array.append(i)


for j in itertools.product(base_array, base_array):
	current_key = "".join(j)
	hash_val = hash_offset(current_key)
	if hash_val not in seen_hashes:
		seen_hashes[hash_val] = current_key
	else:
		print("\n\nCollision\n\n")
		print("key0: " + str(seen_hashes[hash_val]))
		print("key1: " + str(current_key))
		print("hash: " + str(hash_val))

	#print("".join(j))