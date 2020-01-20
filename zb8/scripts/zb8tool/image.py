# Copyright 2019 LaczenJMS
# Copyright 2018 Nordic Semiconductor ASA
# Copyright 2017 Linaro Limited
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""
Image signing and management.
"""

from . import version as versmod
from intelhex import IntelHex
import binascii
import hashlib
import struct
import os.path
import zb8tool.keys as keys
from cryptography.hazmat.primitives.asymmetric import padding
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.backends import default_backend
from cryptography.hazmat.primitives import hashes

BYTE_ALIGNMENT = 8 # Output hex file is aligned to BYTE_ALIGNMENT
MIN_HDRSIZE = 512
HDR_MAGIC = 0x46534c48 # FSLH in hex
HDR_SIZE = 32
VERIFY_HDR_MAGIC = 0x56455249 # VERI in hex
VERIFY_HDR_SIZE = 32
TLV_AREA_SIGNATURE_TYPE = 0
TLV_AREA_SIGNATURE_LENGTH = 64
TLVE_IMAGE_HASH = 0x0100
TLVE_IMAGE_EPUBKEY = 0x0200
TLVE_IMAGE_DEP = 0x0300

INTEL_HEX_EXT = "hex"

STRUCT_ENDIAN_DICT = {
        'little': '<',
        'big':    '>'
}

class Image():
    def __init__(self, hdrsize = None, load_address = None, dest_address = None,
                 version = 0, endian='little', type = 'image'):
        self.hdrsize = hdrsize
        self.load_address = load_address
        self.dest_address = dest_address
        self.run_address = None
        self.version = version or versmod.decode_version("0")
        self.endian = endian
        self.payload = []
        self.size = 0

    def __repr__(self):
        return "<hdrsize={}, load_address={}, dest_address={}, run_address={}, \
                Image version={}, endian={}, type={} format={}, \
                payloadlen=0x{:x}>".format(
                    self.hdrsize,
                    self.load_address,
                    self.dest_address,
                    self.run_address,
                    self.version,
                    self.endian,
                    self.type,
                    self.__class__.__name__,
                    len(self.payload))

    def load(self, path):
        """Load an image from a given file"""
        ext = os.path.splitext(path)[1][1:].lower()

        if ext != INTEL_HEX_EXT:
            raise Exception("Only hex input file supported")
        ih = IntelHex(path)
        self.payload = ih.tobinarray()

        # Padding the payload to aligned size
        if (len(self.payload) % BYTE_ALIGNMENT) != 0:
            padding = BYTE_ALIGNMENT - len(self.payload) % BYTE_ALIGNMENT
            self.payload = bytes(self.payload) + (b'\xff' * padding)

        if self.hdrsize == None:
            self.payload = (b'\x00' * MIN_HDRSIZE) + bytes(self.payload)
            self.hdrsize = MIN_HDRSIZE
            self.run_address = ih.minaddr()
        else:
            self.run_address = ih.minaddr() + self.hdrsize

        if self.dest_address == None:
            self.dest_address = ih.minaddr();

        if self.load_address == None:
            self.load_address = ih.minaddr();

        self.size = len(self.payload) - self.hdrsize;

        self.check()

    def save(self, path):
        """Save an image from a given file"""
        ext = os.path.splitext(path)[1][1:].lower()

        if ext != INTEL_HEX_EXT:
            raise Exception("Only hex input file supported")

        h = IntelHex()
        h.frombytes(bytes=self.payload, offset=self.load_address)
        h.tofile(path, 'hex')

    def check(self):
        """Perform some sanity checking of the image."""
        # Check that image starts with header of all 0x00.
        if any(v != 0x00 for v in self.payload[0:self.hdrsize]):
            raise Exception("Header size provided, but image does not \
            start with 0x00")


    def create(self, signkey, encrkey):

        epubk = None
        if encrkey is not None:

            # Generate new encryption key
            tempkey = keys.EC256P1.generate()
            epubk = tempkey.get_public_key_bytearray()

            # Generate shared secret
            shared_secret = tempkey.gen_shared_secret(encrkey._get_public())

            # Key Derivation function: KDF1
            sha = hashlib.sha256()
            sha.update(shared_secret)
            sha.update(b'\x00\x00\x00\x00')
            plainkey = sha.digest()[:16]

            # Encrypt
            nonce = sha.digest()[16:]
            cipher = Cipher(algorithms.AES(plainkey), modes.CTR(nonce),
                            backend=default_backend())
            encryptor = cipher.encryptor()
            msg = bytes(self.payload[self.hdrsize:])

            enc = encryptor.update(msg) + encryptor.finalize()
            self.payload = bytearray(self.payload)
            self.payload[self.hdrsize:] = enc

        # Calculate the image hash.
        sha = hashlib.sha256()
        sha.update(self.payload[self.hdrsize:])
        hash = sha.digest()

        self.add_header(hash, epubk, signkey)

    def add_header(self, hash, epubk, signkey):
        """Install the image header."""
        # Image info TLV
        e = STRUCT_ENDIAN_DICT[self.endian]
        fmt = (e +
            # type struct zb_fsl_hdr {
            'I' +   # Magic
            'I' +   # Slot Address uint32
            'HBB' + # HDR info - Size, sigtype, siglen
            'I' +   # Size (excluding header)
            'I' +   # Image Address
            'BBHI' +  # Vers ImageVersion
            'I'     # Pad
            ) # }
        hdr = struct.pack(fmt,
                HDR_MAGIC,
                self.load_address,
                self.hdrsize,
                TLV_AREA_SIGNATURE_TYPE,
                TLV_AREA_SIGNATURE_LENGTH,
                self.size,
                self.run_address,
                self.version.major,
                self.version.minor or 0,
                self.version.revision or 0,
                self.version.build or 0,
                0xffffffff
                )

        hdr += struct.pack('H', TLVE_IMAGE_HASH)
        hdr += struct.pack('H', len(hash))
        hdr += hash

        if epubk is not None:
            hdr += struct.pack('H', TLVE_IMAGE_EPUBKEY)
            hdr += struct.pack('H', len(epubk))
            hdr += epubk

        e = STRUCT_ENDIAN_DICT[self.endian]
        fmt = (e +
            # type struct zb_img_dep {
            'I' +   # Image offset
            'BBH' + # Image dep min
            'BBH'   # Image dep max
            ) #}
        dep = struct.pack(fmt,
            self.dest_address + 0x14,
            0,0,0,
            self.version.major or 0,
            self.version.minor or 0,
            self.version.revision or 0
            )

        hdr += struct.pack('H', TLVE_IMAGE_DEP)
        hdr += struct.pack('H', len(dep))
        hdr += dep

        # close the tlv area with a zero byte
        hdr += struct.pack('H', 0)

        hdr_len = self.hdrsize - VERIFY_HDR_SIZE - TLV_AREA_SIGNATURE_LENGTH
        if (len(hdr) > hdr_len):
            raise Exception("Header size to small to fit manifest info")

        # TLVA padding
        while len(hdr) < hdr_len:
            hdr += struct.pack('B', 0xff)

        sha = hashlib.sha256()
        sha.update(hdr)
        hdr_hash = sha.digest()
        hdr_signature = signkey.sign_prehashed(hdr_hash)
        hdr += hdr_signature

        # Verify header
        e = STRUCT_ENDIAN_DICT[self.endian]
        fmt = (e +
            'I' +   # MAGIC
            'I' +   # PAD0
            'I' +   # PAD1
            'I' +   # PAD2
            'I' +   # PAD3
            'I' +   # PAD4
            'I' +   # PAD5
            'I'     # CRC32
        )

        hdr += struct.pack(fmt,
                0xffffffff,
                0xffffffff,
                0xffffffff,
                0xffffffff,
                0xffffffff,
                0xffffffff,
                0xffffffff,
                0xffffffff
        )

        self.payload = bytearray(self.payload)
        self.payload[0:len(hdr)] = hdr