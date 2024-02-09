#include <fstream>
#include <string.h>
#include <iostream>

#include "hyperfs_def.h"

char* name;

// If size = 0
// 		return ftellp;
// Else
// 		read();
// 		return ftellp;
size_t hfs_compliant_read(void* buffer, size_t size, size_t position, void* file_stream)
{
	std::fstream* fst = (std::fstream*)file_stream;
	if (size != 0)
		fst->read((char*)buffer, size);
	return fst->tellp();
}

// If size = 0
// 		If buffer = hfs::HFS_SEEK_SET
// 				fseekp(pos, SEEK_SET);
// 				return ftellp();
// 		Else If buffer = hfs::HFS_SEEK_CUR
// 				fseekp(pos, SEEK_CUR);
// 				return ftellp();
// 		Else If buffer = hfs::HFS_SEEK_END
// 				fseekp(pos, SEEK_END);
// 				return ftellp();
// Else
// 		write();
// 		return ftellp();
size_t hfs_compliant_write(void* buffer, size_t size, size_t position, void* file_stream)
{
	std::fstream* fst = (std::fstream*)file_stream;
	if (size == 0)
	{
		switch (*(uint8_t*)buffer)
		{
			case hfs::HFS_SEEK_SET:
				fst->seekp(position, std::ios::beg);
				return fst->tellp();
			case hfs::HFS_SEEK_CUR:
				fst->seekp(position, std::ios::cur);
				return fst->tellp();
			case hfs::HFS_SEEK_END:
				fst->seekp(position, std::ios::end);
				return fst->tellp();
			default:
				return fst->tellp();
		}
	}
	fst->write((char*)buffer, size);
	return fst->tellp();
}
void hfs_compliant_reset(void* file_stream)
{
	std::fstream* fst = (std::fstream*)file_stream;
	fst->close();
	fst->open(name, std::ios_base::binary | std::ios_base::out | std::ios_base::in | std::ios_base::trunc);
}

void print_ec(int32_t ec)
{
	switch (ec)
	{
		case hfs::ERR_WR_NO_DEF:
			printf("ERR_WR_NO_DEF : NUL_WRF\n");
			break;
		case hfs::ERR_RD_NO_DEF:
			printf("ERR_RD_NO_DEF : NUL_RDF\n");
			break;
		case hfs::ERR_RD_WR_NO_DEF:
			printf("ERR_RD_WR_NO_DEF : NUL_RWF\n");
			break;
		case hfs::ERR_HEADER_INVALID_DIRECTION:
			printf("ERR_HEADER_INVALID_DIRECTION : HED_DIR\n");
			break;
		case hfs::ERR_HEADER_INVALID_CLUSTER_INFO:
			printf("ERR_HEADER_INVALID_CLUSTER_INFO : HED_CIF\n");
			break;
		case hfs::ERR_HEADER_UNSUPPORTED_VERSION:
			printf("ERR_HEADER_UNSUPPORTED_VERSION : HED_VER\n");
			break;
		case hfs::ERR_HEADER_NON_FF_RESERVED_SEGMENT:
			printf("ERR_HEADER_NON_FF_RESERVED_SEGMENT : HED_RSF\n");
			break;
		case hfs::ERR_HEADER_ZERO_BOOT_SIG:
			printf("ERR_HEADER_ZERO_BOOT_SIG : HED_ZBS\n");
			break;
		case hfs::ERR_RFE_INVALID_PRESV_SEGMENT:
			printf("ERR_RFE_INVALID_PRESV_SEGMENT : RFE_PRS\n");
			break;
		case hfs::ERR_RFE_NO_END:
			printf("ERR_RFE_NO_END : RFE_NED\n");
			break;
		case hfs::ERR_DATA_NO_SPACE:
			printf("ERR_DATA_NO_SPACE : DTA_NSP\n");
			break;
		case hfs::ERR_FILE_NOT_LOCKED:
			printf("ERR_FILE_NOT_LOCKED : FIL_NLK\n");
			break;
		case hfs::ERR_FILE_BUFFER_TOO_LARGE:
			printf("ERR_FILE_BUFFER_TOO_LARGE : FIL_BTL\n");
			break;
		case hfs::ERR_FILE_DEPTH_TOO_LARGE:
			printf("ERR_FILE_DEPTH_TOO_LARGE : FIL_DTL\n");
			break;
		default:
			printf("NO ERROR : NO_ERR\n");
			break;
	}
	return;
}

enum pre_f
{
	pf_invalid,
	pf_parse,
	pf_format,
};

enum post_f
{
	invalid,
	add_f,
	write_f,
	read_f,
	vol_d,
	file_d,
	list_f,
};

// Source: https://gist.github.com/dgoguerra/7194777
// Modification: static const char *humanSize -> static const char* human_size
// 				 char *suffix[] = {"B", "KB", "MB", "GB", "TB"} -> const char* suffix[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"}
// 				 sprintf(output, "%.02lf %s", dblBytes, suffix[i]) -> sprintf(output, "%.02lf%s", dblBytes, suffix[i])
static const char* human_size(uint64_t bytes)
{
	const char* suffix[] = {"B", "KiB", "MiB", "GiB", "TiB", "PiB", "EiB"};
	char length = sizeof(suffix) / sizeof(suffix[0]);

	int i = 0;
	double dblBytes = bytes;

	if (bytes > 1024) {
		for (i = 0; (bytes / 1024) > 0 && i<length-1; i++, bytes /= 1024)
			dblBytes = bytes / 1024.0;
	}

	static char output[200];
	sprintf(output, "%.02lf%s", dblBytes, suffix[i]);
	return output;
}

int main(int argc, char** argv)
{
	if (argc < 3)
	{
		printf("Usage: %s <filename> [parse|format <Csz,Cn,Sig,N,A,O,BS0,BS1,LNL[,LN]>] <function>\n", argv[0]);
		return -1;
	}
	pre_f pre_function = pf_invalid;
	if (!memcmp(argv[2], "parse", 5))
		pre_function = pf_parse;
	else if (!memcmp(argv[2], "format", 6))
		pre_function = pf_format;

	if (argc < 5 && !memcmp(argv[2], "parse", 5))
	{
		printf("Functions: \n");
		printf("Add file <Name> <Extention> <Attributes (Hex number)> <Owner ID>\n");
		printf("Write file <Name> <Extention> <file_from>\n");
		printf("Read file <Name> <Extention> <file_to>\n");
		printf("Volume data\n");
		printf("File data <Name> <Extention>\n");
		printf("List files <Show hidden (1/0)>\n");
	}
	else if ((argc != 10 && argc != 11) && !memcmp(argv[2], "format", 6))
	{
		printf("Format: Cluster size (multiple of 4096), Number of Clusters, Signature (Hex), Name (12 characters max), Attributes (Hex), Owner ID, Boot sig 0 (Hex), Boot sig 1 (Hex), Long name length, (if long name length > 0, then long name)\n");
		return -1;
	}
	else if (memcmp(argv[2], "format", 6) && memcmp(argv[2], "parse", 5))
	{
		printf("Error\n");
		printf("Usage: %s <filename> [parse|format <Csz,Cn,Sig,N,A,O,BS0,BS1,LNL[,LN]>] <function>\n", argv[0]);
		int pad_len = strlen(argv[0]) + 20;
		char* pad = (char*)malloc(pad_len);
		memset(pad, 0x20, pad_len - 1); // 0x20 == ' '
		pad[pad_len - 1] = 0;
		printf("%s^^^^^^^^^^^^^ ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n", pad); // Highlights the [parse|format] segment
		return -1;
	}
	name = argv[1];
	std::fstream fst;
	fst.open(name, std::ios_base::binary | std::ios_base::out | std::ios_base::in);

	hfs::hfs_object hfs_o;
	hfs_o.read_fn = hfs_compliant_read;
	hfs_o.write_fn = hfs_compliant_write;
	hfs_o.reset_file_fn = hfs_compliant_reset;
	hfs_o.extra_args = (void*)&fst;

	printf("Initializing hfs_object...\n");
	int32_t ec = hfs_o.init();
	if (ec)
	{
		print_ec(ec);
		fst.close();
		return ec;
	}
	printf("Running pre_function\n");
	post_f post_function = invalid;
	if (pre_function == pf_invalid)
	{
		printf("Pre function invalid\n");
		fst.close();
		return -1;
	}
	else if (pre_function == pf_parse)
	{
		printf("Parsing hfs file...\n");
		hfs_o.parse();
		printf("Running function...\n");
		if (!memcmp(argv[3], "Add", 3) && !memcmp(argv[4], "file", 4))
			post_function = add_f;
		else if (!memcmp(argv[3], "Read", 4) && !memcmp(argv[4], "file", 4))
			post_function = read_f;
		else if (!memcmp(argv[3], "File", 4) && !memcmp(argv[4], "data", 4))
			post_function = file_d;
		else if (!memcmp(argv[3], "List", 4) && !memcmp(argv[4], "files", 5))
			post_function = list_f;
		else if (!memcmp(argv[3], "Write", 5) && !memcmp(argv[4], "file", 4))
			post_function = write_f;
		else if (!memcmp(argv[3], "Volume", 6) && !memcmp(argv[4], "data", 4))
			post_function = vol_d;
		else
		{
			printf("Invalid function, exiting...\n");
			fst.close();
			return -1;
		}
	}
	else if (pre_function == pf_format)
	{
		printf("Formatting hfs file...\n");
		printf("Confirm [y/N]?\n");
		std::string conf_str = "";
		std::getline(std::cin, conf_str);
		if (conf_str != "Y" && conf_str != "y")
		{
			printf("Exiting...\n");
			fst.close();
			return 0;
		}
		printf("Formatting hfs file...\n");
		uint64_t cluster_size = strtoull(argv[3], NULL, 10);
		uint64_t clusters = strtoull(argv[4], NULL, 10);
		uint32_t signature = strtoul(argv[5], NULL, 16);
		uint8_t attribute = strtoul(argv[7], NULL, 16);
		uint8_t owner_id = strtoul(argv[8], NULL, 10);
		uint8_t boot_sig_0 = strtoull(argv[9], NULL, 16);
		uint8_t boot_sig_1 = strtoull(argv[10], NULL, 16);
		uint8_t lname_len = strtoull(argv[11], NULL, 10);
		if (errno == EINVAL)
		{
			printf("Formatting error invalid value '");
			if (cluster_size == 0)
				printf("cluster_size'\n");
			else if (clusters == 0)
				printf("clusters'\n");
			else if (signature == 0)
				printf("signature'\n");
			else if (boot_sig_0 == 0)
				printf("boot_sig_0'\n");
			else if (boot_sig_1 == 0)
				printf("boot_sig_1'\n");
			else if (attribute == 0)
				printf("attribute'\n");
			else if (lname_len == 0 && argc < 11)
				printf("lname_len?'\n");
			else
				printf("unknown'\n");
			fst.close();
			return -1;
		}
		if (lname_len > 0)
		{
			if (argc < 10)
			{
				printf("lname not defined\n");
				fst.close();
				return -1;
			}
		}
		if (boot_sig_0 == 0 || boot_sig_1 == 0)
		{
			print_ec(hfs::ERR_HEADER_ZERO_BOOT_SIG);
			fst.close();
			return hfs::ERR_HEADER_ZERO_BOOT_SIG;
		}
		if ((attribute & 0b01010000) == 0)
		{
			conf_str = "";
			printf("Attributes USER_READ and ROOT_READ are BOTH zero which means the disk CANNOT be read AT ALL,\nDo you want to continue formatting[y/N]?\n");
			std::getline(std::cin, conf_str);
			if (conf_str != "Y" && conf_str != "y")
			{
				printf("Exiting...\n");
				fst.close();
				return -1;
			}
		}
		if (cluster_size % 4096)
		{
			printf("Invalid cluster size, it must be a multiple of 4096\n");
			fst.close();
			return -1;
		}
		uint8_t* name = (uint8_t*)malloc(12);
		memset(name, 0, 12);
		memcpy(name, argv[6], std::min((int)strlen(argv[6]), 12));
		printf("%.12s %.12s\n", name, argv[6]);
		hfs_o.format(cluster_size, clusters, signature, name, attribute, owner_id, boot_sig_0, boot_sig_1, lname_len, lname_len > 0 ? (uint8_t*)argv[11] : nullptr);
		free(name);
		printf("Successfully formatted file.\n");
		fst.close();
		return 0;
	}

	switch (post_function)
	{
		case add_f:
			{
				if (argc < 7)
				{
					printf("Add file <Name> <Extention> <Attributes (Hex number)> <Owner ID>\n");
					fst.close();
					return -1;
				}
				uint8_t attribute = strtoul(argv[7], NULL, 16);
				uint8_t owner_id = strtoull(argv[8], NULL, 10);
				uint8_t* name = (uint8_t*)malloc(12);
				uint8_t* extention = (uint8_t*)malloc(4);
				memset(name, 0, 12);
				memset(extention, 0, 4);
				memcpy(name, argv[5], std::min((int)strlen(argv[4]), 12));
				memcpy(extention, argv[6], std::min((int)strlen(argv[4]), 4));
				hfs_o.add_file(name, extention, attribute, owner_id);
				free(name);
				free(extention);
				break;
			}
		// case write_f:
		// case read_f:
		// case vol_d:
		// case file_d:
		case list_f:
			{
				if (argc < 4)
				{
					printf("List files <Show hidden (1/0)>\n");
					fst.close();
					return -1;
				}
				hfs_o.read_rfe_chain();
				uint8_t is_hidden = strtoul(argv[5], NULL, 10);
				for (size_t i = 0; i < hfs_o.rfe.size(); i++)
				{
					hfs_reserved_file_entry rfe = hfs_o.rfe[i];
					if ((rfe.attribute & 0b00000001) > is_hidden)
						continue;
					printf("-------- %lu %.12s.%.4s\n", i, rfe.name, rfe.extention);
					printf("%c%c%c%c%c%c%c\n", rfe.attribute & 0b01000000 ? 'R' : ' ', rfe.attribute & 0b00100000 ? 'W' : ' ',
							rfe.attribute & 0b00010000 ? 'X' : ' ', rfe.attribute & 0b00001000 ? 'R' : ' ', rfe.attribute & 0b00000100 ? 'W' : ' ',
							rfe.attribute & 0b00001000 ? 'X' : ' ', rfe.attribute & 0b00000001 ? 'H' : ' ');
					printf("%s", rfe.p_resv & 0b10000000 ? "Unsupported, directory\n" : "");
					printf("Clusters: %lu(%s)\n", rfe.cluster_size, human_size(rfe.cluster_size * hfs_o.header.cluster_size));
					hfs::date c_date(rfe.creation_date);
					hfs::date m_date(rfe.modification_date);
					printf("Created: %d/%d/%d, Modified: %d/%d/%d\n",
							c_date.year, c_date.month, c_date.day,
							m_date.year, m_date.month, m_date.day);
					printf("Owner ID: %d", rfe.owner_id);
				}
				printf("--------\n");
				break;
			}
		default:
			printf("Invalid function.\n");
			return -1;
	}

	fst.close();
	return 0;
}
