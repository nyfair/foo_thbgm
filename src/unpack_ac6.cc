#include "SDK/foobar2000.h"

#include <map>

namespace unpack_ac6 {
	struct AC6File {
		t_uint32 pos;		// Position inside the archive
		t_uint32 insize;	// Encrypted file size
		t_uint32 outsize;	// Decrypted file size
	};
	const t_uint32 CP1_SIZE = 0x102;
	const t_uint32 CP2_SIZE = 0x400;
	std::map<pfc::string8, AC6File> files;
	pfc::string8 current_archive;
}
using namespace unpack_ac6;

class archive_ac6 : public archive_impl {
private:
	t_uint32 filecount;
	t_uint32 pool1[CP1_SIZE];
	t_uint32 pool2[CP2_SIZE];

	inline t_uint32 swap_endian(const t_uint32 &x) {
		return ((x & 0x000000ff) << 24) |
				((x & 0x0000ff00) << 8) |
				((x & 0x00ff0000) >> 8) |
				((x & 0xff000000) >> 24);
	}

	void cryptstep(t_uint32 &ecx) {
		static const t_uint32 cmp = (CP1_SIZE - 1);

		pool2[ecx]++;
		ecx++;
		while(ecx <= cmp) {
			pool1[ecx]++;
			ecx++;
		}
		if(pool1[cmp] < 0x10000) return;

		pool1[0] = 0;
		for(int i = 0; i < cmp; i++) {
			pool2[i] = (pool2[i] | 2) >> 1;
			pool1[i + 1] = pool1[i] + pool2[i];
		}
		return;
	}

	bool decrypt(char *dest, const t_uint32 &destsize,
								const char *source, const t_uint32 &sourcesize) {
		t_uint32 ebx = 0, ecx, edi, esi, edx;
		t_uint32 cryptval[2];
		t_uint32 s = 4, d = 0;	// source and destination bytes

		for(int i = 0; i < CP1_SIZE; i++) pool1[i] = i;
		for(int i = 0; i < CP2_SIZE; i++) pool2[i] = 1;
	
		edi = swap_endian(*(t_uint32*)source);
		esi = 0xFFFFFFFF;
	
		while(1) {
			edx = 0x100;
			cryptval[0] = esi / pool1[0x101];
			cryptval[1] = (edi - ebx) / cryptval[0];
			ecx = 0x80;
			esi = 0;

			while(1) {
				while((ecx != 0x100) && (pool1[ecx] > cryptval[1])) {
					ecx--;
					edx = ecx;
					ecx = (esi+ecx) >> 1;
				}
				if(cryptval[1] < pool1[ecx+1]) break;
				esi = ecx+1;
				ecx = (esi+edx) >> 1;
			}

			*(dest + d) = (char)ecx;
			if(++d >= destsize) return true;
			esi = (long)pool2[ecx] * (long)cryptval[0];
			ebx += pool1[ecx] * cryptval[0];
			cryptstep(ecx);
			ecx = (ebx + esi) ^ ebx;

			while(!(ecx & 0xFF000000)) {
				ebx = ebx << 8;
				esi = esi << 8;
				edi = edi << 8;
				ecx = (ebx+esi) ^ ebx;
				edi += *(source + s) & 0x000000FF;
				if(++s >= sourcesize) return true;
			}
		
			while(esi < 0x10000) {
				esi = 0x10000 - (ebx & 0x0000FFFF);
				ebx = ebx << 8;
				esi = esi << 8;
				edi = edi << 8;
				edi += *(source + s) & 0x000000FF;
				if(++s >= sourcesize) return true;
			}
		}
	}

	char* getfileinfo(char *source) {
		AC6File dest;
		pfc::string8 name;
		char *t = source;
		if(*t = '/') t++;	// Jump over directory slash
		t_uint32 fnlen = strlen(t) + 1;
		name.add_string(t, fnlen);
		t += fnlen;
		memcpy(&dest.insize, t, 4);
		t += 4;
		memcpy(&dest.outsize, t, 4);
		t += 4;
		memcpy(&dest.pos, t, 4);
		t += 8;
		files[name] = dest;
		return t;
	}

	void parse_archive(service_ptr_t<file> &m_file,
						const char *p_archive, abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if(stricmp_utf8(current_archive, p_archive)) {
			files.clear();
			current_archive = p_archive;
			char sig[4];
			m_file->read_object_t(sig, p_abort);
			if(memcmp(sig, "PBG6", 4)) throw exception_io_data();

			t_uint32 toc_start, toc_size;
			char *t;
			t_uint32 toc_insize = m_file->get_size(p_abort);
			m_file->read(&toc_start, 4, p_abort);
			m_file->read(&toc_size, 4, p_abort);
			m_file->seek(toc_start, p_abort);
			toc_insize -= toc_start;

			pfc::array_t<char> toc, toc_crypt;
			toc_crypt.set_size(toc_insize);
			toc.set_size(toc_size);
			m_file->read(toc_crypt.get_ptr(), toc_insize, p_abort);
			decrypt(toc.get_ptr(), toc_size, toc_crypt.get_ptr(), toc_insize);
			memcpy(&filecount, toc.get_ptr(), 4);
			t = toc.get_ptr() + 4;

			for(t_uint32 i = 0; i < filecount; i++) {
				t = getfileinfo(t);
			}
		}
	}

public:
	virtual bool supports_content_types() {
		return false;
	}

	virtual const char* get_archive_type() {
		return "ac6";
	}

	virtual t_filestats get_stats_in_archive(const char *p_archive,
							const char *p_file, abort_callback &p_abort) {
		service_ptr_t<file> m_file;
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		t_filestats status(m_file->get_stats(p_abort));
		std::map<pfc::string8, AC6File>::iterator it = files.find(p_file);
		status.m_size = (it == files.end()) ? 0 : it->second.outsize;
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out,const char *p_archive,
								const char *p_file, abort_callback &p_abort) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "ac6")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> enc, dec;
		enc.set_size(files[p_file].insize);
		dec.set_size(files[p_file].outsize);
		m_file->seek(files[p_file].pos, p_abort);
		m_file->read(enc.get_ptr(), files[p_file].insize, p_abort);
		decrypt(dec.get_ptr(), files[p_file].outsize,
			enc.get_ptr(), files[p_file].insize);
		p_out->write(dec.get_ptr(), files[p_file].outsize, p_abort);
	}

	virtual void archive_list(const char *p_archive,
							const service_ptr_t<file> &p_out,
							archive_callback &p_abort, bool p_want_readers) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "ac6")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		pfc::string8 unpackdir = p_archive;
		unpackdir.add_string("_unpack\\");
		if(!filesystem::g_exists(unpackdir, p_abort))
			filesystem::g_create_directory(unpackdir, p_abort);
		std::map<pfc::string8, AC6File>::iterator it;
		for(it = files.begin(); it != files.end(); it++) {
			service_ptr_t<file> out;
			pfc::string8 outfile = unpackdir;
			outfile.add_string(it->first);
			filesystem::g_open_write_new(out, outfile, p_abort);
			pfc::array_t<char> enc, dec;
			enc.set_size(it->second.insize);
			dec.set_size(it->second.outsize);
			m_file->seek(it->second.pos, p_abort);
			m_file->read(enc.get_ptr(), it->second.insize, p_abort);
			decrypt(dec.get_ptr(), it->second.outsize,
				enc.get_ptr(), it->second.insize);
			out->write(dec.get_ptr(), it->second.outsize, p_abort);
		}
	}
};

static archive_factory_t<archive_ac6> g_archive_ac6_factory;
