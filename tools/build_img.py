import os
import argparse

def string_to_16_bytes(string):
    """Convert a string to 16 bytes."""

    # Encode the string to bytes using UTF-8 encoding
    encoded_bytes = string.encode('utf-8')

    # If the encoded bytes are already 16 bytes long, return them
    if len(encoded_bytes) == 16:
        return encoded_bytes

    # If the encoded bytes are shorter than 16 bytes, pad them with zeros
    elif len(encoded_bytes) < 16:
        return encoded_bytes + bytes(16 - len(encoded_bytes))

    # If the encoded bytes are longer than 16 bytes, truncate them
    else:
        return encoded_bytes[:16]
    

def build_image(files, output_file):
    file_sizes = [os.path.getsize(f) for f in files]

    with open(output_file, 'wb') as out_file:
        magic = 0x474D495343565253
        current_offset = 0
        flags = 1
        offsets = []

        header_size = 32 * len(files) + 32
        current_offset += header_size

        for i, file in enumerate(files):
            filename = os.path.basename(file)
            offsets.append((filename, file_sizes[i], current_offset))
            current_offset += file_sizes[i]

        out_file.write(magic.to_bytes(8, 'little'))
        out_file.write(flags.to_bytes(8, 'little'))
        out_file.write(len(files).to_bytes(8, 'little'))
        out_file.write(header_size.to_bytes(8,'little'))

        for filename, size, offset in offsets:
            out_file.write(string_to_16_bytes(filename))
            out_file.write(offset.to_bytes(8, 'little'))
            out_file.write(size.to_bytes(8, 'little'))
            
        for file in files:
            with open(file, 'rb') as f:
                out_file.write(f.read())
                print(f"Processed file {file}")

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Build servers image file")
    parser.add_argument('files', metavar='F', type=str, nargs='+', help="Input files")
    parser.add_argument('-o', '--output', required=True, help="Output file")

    args = parser.parse_args()

    build_image(args.files, args.output)
