#include "SDK/foobar2000.h"

#include <map>
#include "mt.hpp"

namespace unpack_tasfro {
	struct TASFROFile {
		t_uint32 pos;			// Position inside the archive
		t_uint32 size;		// File size
	};
	std::map<pfc::string8, TASFROFile> files;
	pfc::string8 current_archive;
	t_uint16 filecount;
}
using namespace unpack_tasfro;

class archive_tasfro : public archive_impl {
private:
	bool decrypt(char *head, t_uint32 headsize, t_uint32 archivesize) {
		char *t = head;
		t_uint32 offset = 0;
		for(t_uint16 i = 0; i < filecount; ++i) {
			TASFROFile dest;
			memcpy(&dest.pos, t, 4);
			t += 4;
			memcpy(&dest.size, t, 4);
			t += 4;
			t_uint8 name_len = *t;
			t++;
			if(offset + 9 + name_len > archivesize) return false;
			pfc::string8 name(t, name_len);
			files[name] = dest;
			t += name_len;
			offset += 9 + name_len;
			if(dest.pos < headsize + 6 || dest.pos > archivesize) return false;
			if(dest.size > archivesize - dest.pos) return false;
		}
		return true;
	}

	void parse_archive(service_ptr_t<file> &m_file,
										const char *p_archive, abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if(stricmp_utf8(current_archive, p_archive)) {
			files.clear();
			current_archive = p_archive;
			t_filesize archivesize = m_file->get_size(p_abort);
			if(archivesize < 6) throw exception_io_data();
			t_uint32 headsize;
			m_file->read_object_t(filecount, p_abort);
			m_file->read_object_t(headsize, p_abort);
			if(filecount == 0 || headsize == 0 || archivesize < 6 + headsize) 
				throw exception_io_data();

			pfc::array_t<char> head;
			head.set_size(headsize);
			m_file->read(head.get_ptr(), headsize, p_abort);
			RNG_MT mt(headsize + 6);
			for(t_uint32 i = 0; i < headsize; ++i)
				head[i] ^= mt.next_int32() & 0xFF;

			if(!decrypt(head.get_ptr(), headsize, archivesize)) {
				files.clear();
				t_uint8 k = 0xC5, t = 0x83;
				for(t_uint32 i = 0; i < headsize; ++i) {
					head[i] ^= k; k += t; t += 0x53;
				}
				if(!decrypt(head.get_ptr(), headsize, archivesize))
					throw exception_io_data();
			}
		}
	}

public:
	virtual bool supports_content_types() {
		return false;
	}

	virtual const char* get_archive_type() {
		return "tasfro";
	}

	virtual t_filestats get_stats_in_archive(const char *p_archive,
											const char *p_file, abort_callback &p_abort) {
		service_ptr_t<file> m_file;
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		t_filestats status(m_file->get_stats(p_abort));
		std::map<pfc::string8, TASFROFile>::iterator it = files.find(p_file);
		status.m_size = (it == files.end()) ? 0 : it->second.size;
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out,const char *p_archive,
														const char *p_file, abort_callback &p_abort) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "dat")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> buffer;
		buffer.set_size(files[p_file].size);
		m_file->seek(files[p_file].pos, p_abort);
		m_file->read(buffer.get_ptr(), files[p_file].size, p_abort);
		t_uint8 k = (files[p_file].pos >> 1) | 0x23;
		for(t_uint32 i = 0; i < files[p_file].size; ++i) buffer[i] ^= k;
		p_out->write(buffer.get_ptr(), files[p_file].size, p_abort);
	}

	virtual void archive_list(const char *p_archive,
							const service_ptr_t<file> &p_out,
							archive_callback &p_abort, bool p_want_readers) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "dat")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		pfc::string8 unpackdir = p_archive;
		unpackdir.add_string("_unpack\\");
		if(!filesystem::g_exists(unpackdir, p_abort))
			filesystem::g_create_directory(unpackdir, p_abort);
		std::map<pfc::string8, TASFROFile>::iterator it;
		for(it = files.begin(); it != files.end(); it++) {
			service_ptr_t<file> out;
			pfc::string8 outfile = unpackdir;
			pfc::string8 archive_path = it->first;
			archive_path.replace_char('/', '_');
			outfile.add_string(archive_path);
			filesystem::g_open_write_new(out, outfile, p_abort);
			pfc::array_t<char> buffer;
			buffer.set_size(it->second.size);
			m_file->seek(it->second.pos, p_abort);
			m_file->read(buffer.get_ptr(), it->second.size, p_abort);
			t_uint8 k = (it->second.pos >> 1) | 0x23;
			for(t_uint32 i = 0; i < it->second.size; ++i) buffer[i] ^= k;
			out->write(buffer.get_ptr(), it->second.size, p_abort);
		}
	}
};

static archive_factory_t<archive_tasfro> g_archive_tasfro_factory;
