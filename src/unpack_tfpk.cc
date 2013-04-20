#include <map>
#include "config.h"

namespace unpack_tfpk {
	struct TFPKFile {
		t_uint32 pos;		// Position inside the archive
		t_uint32 size;		// File size
	};
	std::map<pfc::string8, TFPKFile> files;
	pfc::string8 current_archive;
	t_uint16 filecount;
}
using namespace unpack_tfpk;

class archive_tfpk : public archive_impl {
private:
	bool decrypt(char *head, t_uint32 headsize, t_uint32 archivesize) {
		return true;
	}

	void parse_archive(service_ptr_t<file> &m_file, const char *p_archive,
						abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if(stricmp_utf8(current_archive, p_archive)) {
			files.clear();
			current_archive = p_archive;
			char sig[4];
			m_file->read_object_t(sig, p_abort);
			if(memcmp(sig, "TFPK", 4)) throw exception_io_data();
		}
	}

public:
	virtual bool supports_content_types() {
		return false;
	}

	virtual const char* get_archive_type() {
		return "tfpk";
	}

	virtual t_filestats get_stats_in_archive(const char *p_archive,
				const char *p_file, abort_callback &p_abort) {
		service_ptr_t<file> m_file;
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		t_filestats status(m_file->get_stats(p_abort));
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out,const char *p_archive,
						const char *p_file, abort_callback &p_abort) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "pak")) {
			throw exception_io_data();
		}
	}

	virtual void archive_list(const char *p_archive,
							const service_ptr_t<file> &p_out,
							archive_callback &p_abort, bool p_want_readers) {
		if(stricmp_utf8(pfc::string_extension(p_archive), "dat")) {
			throw exception_io_data();
		}
	}
};

static archive_factory_t<archive_tfpk> g_archive_tfpk_factory;
