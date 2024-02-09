#pragma once
#include <stdint.h>
#include <functional>

struct hfs_header // 512 bytes (however CLUSTER_SIZE are written in total) 55 + padding[456] + boot_sig (2)
{
	uint32_t signature; // If the signature is "NORD" then it shouldn't be read (such as a boot drive)
	uint8_t direction_b01; // Set to 0x55 0xAA (MSB 55 LSB AA) so that the driver would know if the header and file entry are written in MSB or LSB format.
	uint8_t direction_b10; // If its read as AA55 then its written in LSB format.
	uint64_t cluster_to_be_allocated; // The cluster after the last allocated cluster, after formatting is 0x2 as cluster 0 is the header, even if a long name is used as a long name is stored in cluster 0 (padding) in the
									// following format: [uint8_t SIZE] [uint8_t[SIZE] long_name], And cluster 0x1 is the master RFEC (RFE Chain). It's also the same as the clusters allocated. 0x0 if read-only or full.
	uint64_t cluster_size; // CLUSTER_SIZE
	uint64_t clusters_available; // (Size of disk / CLUSTER_SIZE) - 2 (2 to account for the header and master RFEC)
	uint8_t name[12]; // zero-terminated. Still part of the drive name if a long name is used.
	uint8_t attribute; // LONG NAME (LN) | USER READ (UR) | USER WRITE (UW) | ROOT READ (RR) | ROOT WRITE (RW) | HIDDEN (H) | 2 BIT VERSION (V) // Example: (Norm: 01111000, URO: 01011000, UNA: 00011000, UNAH: 00011100)
	uint16_t creation_date; // 15-9: Year (0: 2024, 2151) 8-5: Month (1-12) 4-0: Day (1-31) | EG: 2024/1/24 0000000|0000|11000
	uint8_t owner_id;
	uint8_t reserved; // Could be used in later versions. In version 0 it's always 0xFF
	uint64_t clusters;
	uint8_t padding[456]; // 456
	uint8_t boot_sig_0; // If bootable then it's set to 0x55AA, if not the anything else other than zero.
	uint8_t boot_sig_1; // If bootable then it's set to 0x55AA, if not the anything else other than zero.
	// Extra padding to fill in the rest of the cluster
	uint8_t* c_pad; // malloc(CLUSTER_SIZE - HEADER_SIZE)
}__attribute__((packed));

struct hfs_reserved_file_entry // 40 bytes, stored in reserved clusters with the last entry being a special reserved_chain_entry pointing to a cluster with another reserved_file_entry chain.
{						//	 reserved clusters being cluster 1 and any clusters containing rfe_chains pointed to by reserved_chain_entry(s)
	uint8_t name[12]; // zero-terminated.
	uint8_t extention[4]; // While the extention field is zero-terminated if the extention is 4 bytes long it is NOT zero-terminated
	uint8_t attribute; // LONG NAME (LN) | USER READ (UR) | USER WRITE (UW) | USER EXECUTE (UX) | ROOT READ (RR) | ROOT WRITE (RW) | ROOT EXECUTE (RX) | HIDDEN (H) // Long names are stored inside the first cluster in
						// the following format: [uint8_t SIZE] [uint8_t[SIZE] long_name]; The extention is determined by [name+extention] ONLY WHEN using a long name and is zero-terminated ONLY
						// WHEN it is NOT 16 bytes long.
	uint8_t p_resv; // Most significant bit determines if this is a directory or not. Second most significant bit is always 0
					// (after the long name if defined) which is of size 8 bytes (uint64_t). If its a directory it points to an RFE chain. The rest of the bits are 1 except the last one which determines if this is a deleted rfe if its 0x3E or 0x5E (DIR).
	uint64_t cluster_size;
	uint16_t creation_date; // 15-9: Year (0: 2024, 2151) 8-5: Month (1-12) 4-0: Day (1-31) | EG: 2024/1/24 0000000|0000|11000
	uint16_t modification_date; // EG: 2027/5/20 0000011|0100|10100
	uint8_t owner_id;
	uint8_t is_last_rfe;
	uint64_t next_cluster; // Points to first cluster for file data.
}__attribute__((packed));

struct hfs_reserved_chain_entry // 24 bytes
{
	uint64_t next_rfe_chain; // if it doesn't point to an rfe_chain then CLUSTER_END
	uint8_t reserved[16];
};

namespace hfs
{
	const int32_t 							 ERR_WR_NO_DEF = -1; //NUL_WRF
	const int32_t 							 ERR_RD_NO_DEF = -2; //NUL_RDF
	const int32_t 						  ERR_RD_WR_NO_DEF = -3; //NUL_RWF // NOTE: Trigger if reset_fn is not set.
	const int32_t			  ERR_HEADER_INVALID_DIRECTION = -4; //HED_DIR
	const int32_t		   ERR_HEADER_INVALID_CLUSTER_INFO = -5; //HED_CIF
	const int32_t			ERR_HEADER_UNSUPPORTED_VERSION = -6; //HED_VER
	const int32_t		ERR_HEADER_NON_FF_RESERVED_SEGMENT = -7; //HED_RSF
	const int32_t				  ERR_HEADER_ZERO_BOOT_SIG = -8; //HED_ZBS
	const int32_t			 ERR_RFE_INVALID_PRESV_SEGMENT = -9; //RFE_PRS
	const int32_t							ERR_RFE_NO_END = -10;//RFE_NED
	const int32_t						 ERR_DATA_NO_SPACE = -11;//DTA_NSP
	const int32_t					   ERR_FILE_NOT_LOCKED = -12;//FIL_NLK
	const int32_t				 ERR_FILE_BUFFER_TOO_LARGE = -13;//FIL_BTL
	const int32_t				  ERR_FILE_DEPTH_TOO_LARGE = -14;//FIL_DTL

	const uint8_t HFS_SEEK_SET = 0;
	const uint8_t HFS_SEEK_CUR = 1;
	const uint8_t HFS_SEEK_END = 2;

	struct date
	{
		date()
		{
			year = 0;
			month = day = 0;
		}
	
		date(uint16_t compact)
		{
			year = ((compact >> 7) & 0x007F) + 2024;
			month = ((compact >> 5) & 0x000F) + 1;
			day = compact & 0x001F;
		}
	
		uint16_t year;
		uint8_t month;
		uint8_t day;
	};

	struct hfs_object
	{
		// Buffer, size, position, extra_args
		// if size == 0
		// 		return position
		std::function<size_t(void*, size_t, size_t, void*)> read_fn;
		// Buffer, size, position, extra_args
		// if size == 0
		// 		set position with SEEK_MODE in buffer.
		std::function<size_t(void*, size_t, size_t, void*)> write_fn;
		// extra_args
		// Truncates file.
		std::function<void(void*)> reset_file_fn;

		void* extra_args;
		hfs_header header;
		std::vector<hfs_reserved_file_entry> rfe;
		std::vector<uint64_t> lock_rfe;
		size_t position;

		int no_read;
		int bootable;

		int32_t init();
		int uninit();
		int32_t parse();
		int32_t read_rfe_chain();
		int32_t write_rfe_chain();
		// Name is 12 bytes;
		int format(uint64_t cluster_size, uint64_t clusters, uint32_t signature, uint8_t* name, uint8_t attributes, uint8_t owner_id, uint8_t boot_sig_0, uint8_t boot_sig_1, uint8_t lname_len, uint8_t* lname);
		// Name is 12 bytes; Extention is 4 bytes;
		int32_t add_file(uint8_t* name, uint8_t* extention, uint8_t attribute, uint8_t owner_id);
		// Returns 0 when failed.
		uint64_t lock_file(uint8_t* name, uint8_t* extention);
		int32_t unlock_file(uint64_t fptr);
		// Buffer max size is cluster_size - position - 10 // Depth: How many next_cluster chains will it seek before writing // Ex_buff: adds an extra buffer and seeks to it before writing (unless size is 0)
		int32_t write_buff(uint64_t fptr, void* buffer, uint64_t size, uint64_t position, uint64_t depth, int ex_buff);
		int32_t read_buff(uint64_t fptr, void* buffer, uint64_t size, uint64_t position, uint64_t depth);
		// auth_level 0 = user 1 = root/owner
		int f_can_read(uint64_t fptr, int auth_level);
		int f_can_write(uint64_t fptr, int auth_level);
		int f_can_execute(uint64_t fptr, int auth_level);
		int f_is_hidden(uint64_t fptr);
		uint16_t f_creation_date(uint64_t fptr);
		uint16_t f_modification_date(uint64_t fptr);
		uint8_t f_get_owner(uint64_t fptr);
		// 12 bytes 4 bytes
		void f_get_name(uint64_t fptr, uint8_t* name, uint8_t* extention);
		void f_set_read(uint64_t fptr, int auth_level, int val);
		void f_set_write(uint64_t fptr, int auth_level, int val);
		void f_set_execute(uint64_t fptr, int auth_level, int val);
		void f_set_hidden(uint64_t fptr, int val);
		void f_set_owner(uint64_t fptr, uint8_t owner);
		void f_set_name(uint64_t fptr, uint8_t* name, uint8_t* extention);
		uint16_t vol_creation_date();
		uint64_t vol_size();
		// 12 bytes
		void vol_get_name(uint8_t* name);
		void vol_set_name(uint8_t* name);
		uint8_t vol_get_version();
		int vol_can_read(int auth_level);
		int vol_can_write(int auth_level);
		int vol_is_hidden();
		void vol_set_read(int auth_level, int val);
		void vol_set_write(int auth_level, int val);
		void vol_set_hidden(int val);
		void vol_set_owner(uint8_t owner);
	};
}
