#include "SDK/foobar2000.h"
#include "helpers/helpers.h"
#include <map>
#include <string>
#include <vector>
using namespace std;
#include "thxmlparser.h"
#include "config.h"

#define needloop (loopforever || current_loop < loopcount)
static mainmenu_group_factory g_mainmenu_group(g_mainmenu_group_id, 
	mainmenu_groups::playback, mainmenu_commands::sort_priority_dontcare);
static cfg_bool loopforever(cfg_loop_forever, true);
static cfg_uint loopcount(cfg_loop_count, 1);
static cfg_bool read_thbgm_info(cfg_thbgm_readinfo, false);
static cfg_bool dump_thbgm(cfg_thbgm_dump, false);

const t_uint32 deltaread = 1024;
double seek_seconds;
t_uint32 bits;
t_uint32 channels;
t_uint32 samplewidth;
t_uint32 samplerate;
string pack;
t_filesize m_offset;
t_filesize m_headlen;
t_filesize m_looplen;
t_filesize m_totallen;
t_filesize current_sample;
t_uint32 current_loop;
t_filesize internaloffset;
t_filesize internalsize;
vector<map<string, string>> filelist;

inline t_uint32 swap_endian(const t_uint32 &x) {
	return ((x & 0x000000ff) << 24) |
			((x & 0x0000ff00) << 8) |
			((x & 0x00ff0000) >> 8) |
			((x & 0xff000000) >> 24);
}

inline t_uint8 char2int(const char input) {
	if (input >= '0' && input <= '9')
		return input - '0';
	if (input >= 'A' && input <= 'F')
		return input - 'A' + 10;
}

class mainmenu_loopsetting : public mainmenu_commands {
public:
	enum {
		loop_forever,
		loop_count,
		thbgm_readinfo,
		thbgm_dump,
		count
	};

	t_uint32 get_command_count() {
		return count;
	}

	GUID get_command(t_uint32 p_index) {
		switch(p_index) {
			case loop_forever: return guid_loop_forever;
			case loop_count: return guid_loop_count;
			case thbgm_readinfo : return guid_thbgm_readinfo;
			case thbgm_dump : return guid_thbgm_dump;
		}
	}

	void get_name(t_uint32 p_index, pfc::string_base &p_out) {
		switch(p_index) {
			case loop_forever:
				p_out = "ThBGM Loop Forever";
				break;
			case loop_count: 
				p_out = "ThBGM LoopX";
				char counts[12];
				_itoa_s(loopcount, counts, 10);
				p_out.add_string(counts);
				break;
			case thbgm_readinfo:
				p_out = "ThBGM Read Metadata";
				break;
			case thbgm_dump:
				p_out = "ThBGM Extract Files";
				break;
		}
	}

	bool get_description(t_uint32 p_index, pfc::string_base &p_out) {
		switch(p_index) {
			case loop_forever:
				p_out = "infinity loop mode";
				return true;
			case loop_count:
				p_out = "specify loop count";
				return true;
			case thbgm_readinfo:
				p_out = "read bgm's metadata from original file";
				return true;
			case thbgm_dump:
				p_out = "extract bgm files under current directory";
				return true;
		}
	}

	GUID get_parent() {
		return g_mainmenu_group_id;
	}

	bool get_display(t_uint32 p_index, pfc::string_base &p_text, t_uint32 &p_flags) {
		get_name(p_index, p_text);
		switch(p_index) {
			case loop_forever:
				p_flags = loopforever ? mainmenu_commands::flag_radiochecked : 0;
				break;
			case loop_count:
				p_flags = loopforever ? 0 : mainmenu_commands::flag_radiochecked;
				break;
			case thbgm_readinfo:
				p_flags = read_thbgm_info ? mainmenu_commands::flag_checked : 0;
				break;
			case thbgm_dump:
				p_flags = dump_thbgm ? mainmenu_commands::flag_checked : 0;
				break;
		}
		return true;
	}

	void execute(t_uint32 p_index,service_ptr_t<service_base> p_callback) {
		switch(p_index) {
			case loop_forever:
				loopforever = true;
				break;
			case loop_count:
				loopforever = false;
				loopcount = atoi(_InputBox("Please specify loop counts"));
				if (!loopcount || loopcount == 0) loopcount = 1;
				if (loopcount > 65535 || loopcount < 0) loopcount = 65535;
				break;
			case thbgm_readinfo:
				read_thbgm_info = !read_thbgm_info;
				break;
			case thbgm_dump:
				dump_thbgm = !dump_thbgm;
				break;
		}
	}
};
static mainmenu_commands_factory_t<mainmenu_loopsetting> loopsetting_factory;

class input_raw {
private:
	file::ptr m_file;
	input_decoder::ptr decoder;
	bool first_packet;
public:
	void open(const char *p_path, t_input_open_reason p_reason,
				bool isWave, abort_callback &p_abort) {
		m_file.release();
		if (isWave) {
			input_open_file_helper(m_file, p_path, p_reason, p_abort);
		} else {
			decoder.release();
			input_entry::g_open_for_decoding(decoder, m_file, p_path, p_abort);
		}
	}

	t_size read(void *buffer, t_size size, abort_callback &p_abort) {
		return m_file->read(buffer, size, p_abort);
	}

	void raw_seek(t_filesize size, abort_callback &p_abort) {
		m_file->seek(size, p_abort);
	}

	void get_info(file_info &p_info, abort_callback &p_abort) {
		decoder->get_info(0, p_info, p_abort);
	}

	bool run(audio_chunk &p_chunk, abort_callback &p_abort) {
		if (first_packet) {
			decoder->initialize(0, 4, p_abort);
			double time_offset = audio_math::samples_to_time(m_offset, samplerate);
			decoder->seek(seek_seconds + time_offset, p_abort);
			first_packet = false;
		}
		
		bool result = decoder->run(p_chunk, p_abort);
		if (needloop) {
			t_size read_count = p_chunk.get_sample_count();
			current_sample += read_count;
			if (current_sample >= m_totallen || !result) {
				t_size redundant_sample = current_sample - m_totallen;
				p_chunk.set_sample_count(read_count - redundant_sample);
				p_chunk.set_data_size((read_count - redundant_sample) * p_chunk.get_channel_count());
				current_sample = m_headlen;
				seek(audio_math::samples_to_time(m_offset + m_headlen, samplerate));
				if (!loopforever) current_loop++;
			}
		}
		return result;
	}

	void seek(double seconds) {
		current_sample = audio_math::time_to_samples(seconds, samplerate);
		seek_seconds = seconds;
		first_packet = true;
	}
};

struct AC6File {
	t_uint32 pos;		// Position inside the archive
	t_uint32 insize;	// Encrypted file size
	t_uint32 outsize;	// Decrypted file size
};
const t_uint32 CP1_SIZE = 0x102;
const t_uint32 CP2_SIZE = 0x400;
map<pfc::string8, AC6File> ac6files;
pfc::string8 ac6archive;

class archive_ac6 : public archive_impl {
private:
	t_uint32 filecount;
	t_uint32 pool1[CP1_SIZE];
	t_uint32 pool2[CP2_SIZE];

	void cryptstep(t_uint32 &ecx) {
		static const t_uint32 cmp = (CP1_SIZE - 1);

		pool2[ecx]++;
		ecx++;
		while (ecx <= cmp) {
			pool1[ecx]++;
			ecx++;
		}
		if (pool1[cmp] < 0x10000) return;

		pool1[0] = 0;
		for (int i = 0; i < cmp; i++) {
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

		for (int i = 0; i < CP1_SIZE; i++) pool1[i] = i;
		for (int i = 0; i < CP2_SIZE; i++) pool2[i] = 1;
	
		edi = swap_endian(*(t_uint32*)source);
		esi = 0xFFFFFFFF;
	
		while (1) {
			edx = 0x100;
			cryptval[0] = esi / pool1[0x101];
			cryptval[1] = (edi - ebx) / cryptval[0];
			ecx = 0x80;
			esi = 0;

			while (1) {
				while ((ecx != 0x100) && (pool1[ecx] > cryptval[1])) {
					ecx--;
					edx = ecx;
					ecx = (esi+ecx) >> 1;
				}
				if (cryptval[1] < pool1[ecx+1]) break;
				esi = ecx+1;
				ecx = (esi+edx) >> 1;
			}

			*(dest + d) = (char)ecx;
			if (++d >= destsize) return true;
			esi = (long)pool2[ecx] * (long)cryptval[0];
			ebx += pool1[ecx] * cryptval[0];
			cryptstep(ecx);
			ecx = (ebx + esi) ^ ebx;

			while (!(ecx & 0xFF000000)) {
				ebx = ebx << 8;
				esi = esi << 8;
				edi = edi << 8;
				ecx = (ebx+esi) ^ ebx;
				edi += *(source + s) & 0x000000FF;
				if (++s >= sourcesize) return true;
			}
		
			while (esi < 0x10000) {
				esi = 0x10000 - (ebx & 0x0000FFFF);
				ebx = ebx << 8;
				esi = esi << 8;
				edi = edi << 8;
				edi += *(source + s) & 0x000000FF;
				if (++s >= sourcesize) return true;
			}
		}
	}

	char* getfileinfo(char *source) {
		AC6File dest;
		pfc::string8 name;
		char *t = source;
		if (*t = '/') t++;	// Jump over directory slash
		t_uint32 fnlen = strlen(t) + 1;
		name.add_string(t, fnlen);
		t += fnlen;
		memcpy(&dest.insize, t, 4);
		t += 4;
		memcpy(&dest.outsize, t, 4);
		t += 4;
		memcpy(&dest.pos, t, 4);
		t += 8;
		ac6files[name] = dest;
		return t;
	}

	void parse_archive(service_ptr_t<file> &m_file,
			const char *p_archive, abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if (stricmp_utf8(ac6archive, p_archive)) {
			ac6files.clear();
			ac6archive = p_archive;
			char sig[4];
			m_file->read_object_t(sig, p_abort);
			if (memcmp(sig, "PBG6", 4)) throw exception_io_data();

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

			for (t_uint32 i = 0; i < filecount; i++) {
				t = getfileinfo(t);
			}
		}

		if (dump_thbgm) {
			pfc::string8 unpackdir = p_archive;
			unpackdir.add_string("_unpack\\");
			if (!filesystem::g_exists(unpackdir, p_abort))
				filesystem::g_create_directory(unpackdir, p_abort);
			map<pfc::string8, AC6File>::iterator it;
			for (it = ac6files.begin(); it != ac6files.end(); it++) {
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
			dump_thbgm = false;
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
		map<pfc::string8, AC6File>::iterator it = ac6files.find(p_file);
		status.m_size = (it == ac6files.end()) ? 0 : it->second.outsize;
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out, const char *p_archive,
				const char *p_file, abort_callback &p_abort) {
		if (stricmp_utf8(pfc::string_extension(p_archive), "ac6")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> enc, dec;
		enc.set_size(ac6files[p_file].insize);
		dec.set_size(ac6files[p_file].outsize);
		m_file->seek(ac6files[p_file].pos, p_abort);
		m_file->read(enc.get_ptr(), ac6files[p_file].insize, p_abort);
		decrypt(dec.get_ptr(), ac6files[p_file].outsize,
			enc.get_ptr(), ac6files[p_file].insize);
		p_out->write(dec.get_ptr(), ac6files[p_file].outsize, p_abort);
	}

	virtual void archive_list(const char *p_archive,
				const service_ptr_t<file> &p_out,
				archive_callback &p_abort, bool p_want_readers) {
		throw exception_io_data();
	}
};
static archive_factory_t<archive_ac6> g_archive_ac6_factory;

// Mersenne Twister
class RNG_MT {
private:
  enum {
    N = 624,
    M = 397,
    MATRIX_A = 0x9908b0dfUL,
    UPPER_MASK = 0x80000000UL,
    LOWER_MASK = 0x7FFFFFFFUL
  };

  unsigned int mt[N];
  int mti;

public:
  explicit RNG_MT(unsigned int s) {
    init(s);
  }

  void init(unsigned int s) {
    mt[0] = s;
    for (mti = 1; mti < N; ++mti) {
      mt[mti] =
        (1812433253UL * (mt[mti-1] ^ (mt[mti-1] >> 30)) + mti);
    }
  }

  unsigned int next_int32() {
    unsigned int y;
    static const unsigned int mag01[2]={0x0UL, MATRIX_A};
    if (mti >= N) {
      int kk;
      if (mti == N+1)
          init(5489UL);

      for (kk=0;kk<N-M;kk++) {
        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
        mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1UL];
      }
      for (;kk<N-1;kk++) {
        y = (mt[kk]&UPPER_MASK)|(mt[kk+1]&LOWER_MASK);
        mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1UL];
      }
      y = (mt[N-1]&UPPER_MASK)|(mt[0]&LOWER_MASK);
      mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1UL];

      mti = 0;
    }
  
    y = mt[mti++];

    /* Tempering */
    y ^= (y >> 11);
    y ^= (y << 7) & 0x9d2c5680UL;
    y ^= (y << 15) & 0xefc60000UL;
    y ^= (y >> 18);

    return y;
  }
};

struct TASFROFile {
	t_uint32 pos;		// Position inside the archive
	t_uint32 size;		// File size
};
map<pfc::string8, TASFROFile> tasfrofiles;
pfc::string8 tasfroarchive;

class archive_tasfro : public archive_impl {
private:
	t_uint16 filecount;
	bool decrypt(char *head, t_uint32 headsize, t_uint32 archivesize) {
		char *t = head;
		t_uint32 offset = 0;
		for (t_uint16 i = 0; i < filecount; ++i) {
			TASFROFile dest;
			memcpy(&dest.pos, t, 4);
			t += 4;
			memcpy(&dest.size, t, 4);
			t += 4;
			t_uint8 name_len = *t;
			t++;
			if (offset + 9 + name_len > archivesize) return false;
			pfc::string8 name(t, name_len);
			tasfrofiles[name] = dest;
			t += name_len;
			offset += 9 + name_len;
			if (dest.pos < headsize + 6 || dest.pos > archivesize) return false;
			if (dest.size > archivesize - dest.pos) return false;
		}
		return true;
	}

	void dump(pfc::array_t<char> &buffer, TASFROFile f, service_ptr_t<file> &m_file, abort_callback &p_abort) {
		t_uint32 size = (f.size + 3) >> 2;
		buffer.set_size(size << 2);
		m_file->seek(f.pos, p_abort);
		m_file->read(buffer.get_ptr(), f.size, p_abort);
		t_uint32 *buf = (t_uint32*) (buffer.get_ptr());
		t_uint8 k = (f.pos >> 1) | 0x23;
		t_int32 kk = k + (k << 8) + (k << 16) + (k << 24);
		for (t_uint32 i = 0; i < size; ++i) buf[i] ^= kk;
	}

	void parse_archive(service_ptr_t<file> &m_file, const char *p_archive,
				abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if (stricmp_utf8(tasfroarchive, p_archive)) {
			tasfrofiles.clear();
			tasfroarchive = p_archive;
			t_filesize archivesize = m_file->get_size(p_abort);
			if (archivesize < 6) throw exception_io_data();
			t_uint32 headsize;
			m_file->read_object_t(filecount, p_abort);
			m_file->read_object_t(headsize, p_abort);
			if (filecount == 0 || headsize == 0 || archivesize < 6 + headsize) 
				throw exception_io_data();

			pfc::array_t<char> head;
			head.set_size(headsize);
			m_file->read(head.get_ptr(), headsize, p_abort);
			RNG_MT mt(headsize + 6);
			for (t_uint32 i = 0; i < headsize; ++i)
				head[i] ^= mt.next_int32() & 0xFF;

			if (!decrypt(head.get_ptr(), headsize, archivesize)) {
				tasfrofiles.clear();
				t_uint8 k = 0xC5, t = 0x83;
				for (t_uint32 i = 0; i < headsize; ++i) {
					head[i] ^= k; k += t; t += 0x53;
				}
				if (!decrypt(head.get_ptr(), headsize, archivesize))
					throw exception_io_data();
			}
		}

		if (dump_thbgm) {
			pfc::string8 unpackdir = p_archive;
			unpackdir.add_string("_unpack");
			map<pfc::string8, TASFROFile>::iterator it;
			for (it = tasfrofiles.begin(); it != tasfrofiles.end(); it++) {
				service_ptr_t<file> out;
				pfc::string8 outfile = unpackdir;
				pfc::string8 archive_path = it->first;
				char directory[255];
				strncpy(directory, archive_path.toString(), archive_path.length());

				char *parts;
				directory[archive_path.length()] = 0;
				parts = strtok(directory, "/");
				while (parts != NULL) {
					outfile.add_string("\\");
					if (!filesystem::g_exists(outfile, p_abort))
						filesystem::g_create_directory(outfile, p_abort);
					outfile.add_string(parts);
					parts = strtok(NULL, "/");
				} 

				filesystem::g_open_write_new(out, outfile, p_abort);
				pfc::array_t<char> buffer;
				dump(buffer, it->second, m_file, p_abort);
				out->write(buffer.get_ptr(), it->second.size, p_abort);
			}
			dump_thbgm = false;
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
		map<pfc::string8, TASFROFile>::iterator it = tasfrofiles.find(p_file);
		status.m_size = (it == tasfrofiles.end()) ? 0 : it->second.size;
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out, const char *p_archive,
				const char *p_file, abort_callback &p_abort) {
		if (stricmp_utf8(pfc::string_extension(p_archive), "dat")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> buffer;
		dump(buffer, tasfrofiles[p_file], m_file, p_abort);
		p_out->write(buffer.get_ptr(), tasfrofiles[p_file].size, p_abort);
	}

	virtual void archive_list(const char *p_archive,
				const service_ptr_t<file> &p_out,
				archive_callback &p_abort, bool p_want_readers) {
		throw exception_io_data();
	}
};
static archive_factory_t<archive_tasfro> g_archive_tasfro_factory;

struct TFPKFile {
	t_uint32 pos;		// Position inside the archive
	t_uint32 size;		// File size
	t_uint8 key[16];	// Dump key
};
map<pfc::string8, TFPKFile> tfpkfiles;
pfc::string8 tfpkarchive;

class archive_tfpk : public archive_impl {
private:
	t_uint8 tfpk_ver;

	void dump(pfc::array_t<char> &buffer, TFPKFile f, service_ptr_t<file> &m_file, abort_callback &p_abort) {
		t_uint32 size = (f.size + 3) >> 2;
		buffer.set_size(size << 2);
		m_file->seek(f.pos, p_abort);
		m_file->read(buffer.get_ptr(), f.size, p_abort);
		t_uint32 *buf = (t_uint32*) (buffer.get_ptr()), *key, c, k;
		t_uint8 j = 0;
		switch (tfpk_ver) {
			case 0:		// th135
				for (t_uint32 i = 0; i < size; ++i) {
					buf[i] ^= key[j];
					j = (j + 1) & 3;
				}
				break;
			case 1:	// th145 & th155
				key = (t_uint32*)f.key;
				c = key[0];
				for (t_uint32 i = 0; i < size; ++i) {
					k = key[j] ^ buf[i];
					k ^= c;
					c = buf[i];
					buf[i] = k;
					j = (j + 1) & 3;
				}
				break;
			default: // th175
				c = f.size ^ f.pos;
				for (t_uint32 i = 0; i < f.size; i += 4) {
					t_uint32 x = 0;
					t_uint32 y = c + i/4;
					for (j = 0; j < 4; j++) {
						int64_t a = y * 0x5E4789C9ll;
						t_uint32 b = (a >> 0x2E) + (a >> 0x3F);
						t_uint32 t = (y - b * 0xADC8) * 0xBC8F + b * 0xFFFFF2B9;
						if ((int32_t)t <= 0) {
							t += 0x7FFFFFFF;
						}
						y = t;
						x = (x << 8) | (y & 0xFF);
					}
					if (i + 4 < f.size) {
						buf[i/4] ^= x;
					} else {
						for (j = 0; j < f.size-i; j++)
							buffer.get_ptr()[i + j] ^= x >> (8 * j);
					}
				}
		}
	}

	void parse_archive(service_ptr_t<file> &m_file, const char *p_archive,
		abort_callback &p_abort) {
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		if (stricmp_utf8(tfpkarchive, p_archive)) {
			tfpkfiles.clear();
			tfpkarchive = p_archive;
			if (tfpk_ver == NULL) {
				char sig[4];
				m_file->read_object_t(sig, p_abort);
				m_file->read_object_t(tfpk_ver, p_abort);
				if (memcmp(sig, "TFPK", 4)) throw exception_io_data();
			}
			for (size_t i = 0; i < filelist.size(); i++) {
				map<string, string> f = filelist[i];
				TFPKFile file = { _atoi64(f["pos"].c_str()), _atoi64(f["len"].c_str()) };
				if (tfpk_ver != 175) {
					const char* keyhex = f["key"].c_str();
					for (int i = 0; i < 16; i++)
						file.key[i] = (char2int(*(keyhex + (i << 1))) << 4) + char2int(*(keyhex + (i << 1) + 1));
				}
				tfpkfiles[f["name"].c_str()] = file;
			}
		}

		if (dump_thbgm) {
			pfc::string8 unpackdir = p_archive;
			unpackdir.add_string("_unpack");
			std::map<pfc::string8, TFPKFile>::iterator it;
			for (it = tfpkfiles.begin(); it != tfpkfiles.end(); it++) {
				service_ptr_t<file> out;
				pfc::string8 outfile = unpackdir;
				pfc::string8 archive_path = it->first;
				char directory[255];
				strncpy(directory, archive_path.toString(), archive_path.length());

				char *parts;
				directory[archive_path.length()] = 0;
				parts = strtok(directory, "/");
				while (parts != NULL) {
					outfile.add_string("\\");
					if (!filesystem::g_exists(outfile, p_abort))
						filesystem::g_create_directory(outfile, p_abort);
					outfile.add_string(parts);
					parts = strtok(NULL, "/");
				}

				filesystem::g_open_write_new(out, outfile, p_abort);
				pfc::array_t<char> buffer;
				dump(buffer, it->second, m_file, p_abort);
				out->write(buffer.get_ptr(), it->second.size, p_abort);
			}
			dump_thbgm = false;
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
		std::map<pfc::string8, TFPKFile>::iterator it = tfpkfiles.find(p_file);
		status.m_size = (it == tfpkfiles.end()) ? 0 : it->second.size;
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out, const char *p_archive,
		const char *p_file, abort_callback &p_abort) {
		const char* ext = pfc::string_extension(p_archive);
		if (ext[0] == 'c' && ext[1] == 'g') {
			tfpk_ver = 175;
		} else if (stricmp_utf8(ext, "pak")) {
			throw exception_io_data();
		}
		service_ptr_t<file> m_file;
		parse_archive(m_file, p_archive, p_abort);

		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> buffer;
		dump(buffer, tfpkfiles[p_file], m_file, p_abort);
		p_out->write(buffer.get_ptr(), tfpkfiles[p_file].size, p_abort);
	}

	virtual void archive_list(const char *p_archive,
		const service_ptr_t<file> &p_out,
		archive_callback &p_abort, bool p_want_readers) {
		throw exception_io_data();
	}
};

static archive_factory_t<archive_tfpk> g_archive_tfpk_factory;

class raw_binary : public archive_impl {
public:
	virtual bool supports_content_types() {
		return false;
	}

	virtual const char* get_archive_type() {
		return "raw";
	}

	virtual t_filestats get_stats_in_archive(const char *p_archive,
				const char *p_file, abort_callback &p_abort) {
		service_ptr_t<file> m_file;
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		t_filestats status(m_file->get_stats(p_abort));
		return status;
	}

	virtual void open_archive(service_ptr_t<file> &p_out, const char *p_archive,
				const char *p_file, abort_callback &p_abort) {
		service_ptr_t<file> m_file;
		filesystem::g_open(m_file, p_archive, filesystem::open_mode_read, p_abort);
		filesystem::g_open_tempmem(p_out, p_abort);
		pfc::array_t<char> buffer;
		buffer.set_size(internalsize);
		m_file->seek(internaloffset, p_abort);
		m_file->read(buffer.get_ptr(), internalsize, p_abort);
		p_out->write(buffer.get_ptr(), internalsize, p_abort);
	}

	virtual void archive_list(const char *p_archive,
				const service_ptr_t<file> &p_out,
				archive_callback &p_abort, bool p_want_readers) {
		throw exception_io_data();
	}
};
static archive_factory_t<raw_binary> g_raw_binary_factory;

class input_thxml {
protected:
	file::ptr m_file;
	pfc::string8 basepath;
	vector<map<string, string>> bgmlist;
	pfc::string8 title;
	pfc::string8 artist;
	pfc::string8 encoding;
	pfc::string8 codec;
	t_filesize m_filepos;
	t_filesize m_maxseeksize;
	pfc::string8 totaltracks;
	
	bool isWave;
	bool isArchive;
	bool isStream;
	input_raw raw;
	pfc::array_t<t_uint8> m_buffer;

	inline t_uint64 length_to_samples(t_filesize p_length) {
		return p_length / samplewidth;
	}

	inline t_filesize samples_to_length(t_uint64 p_samples) {
		return p_samples * samplewidth;
	}

	void open_raw(t_uint32 p_subsong, abort_callback &p_abort) {
		string bgm_path = bgmlist[p_subsong]["file"];
		string path;
		if (isStream) {
			string filepos;
			size_t name_pos, pos_size;
			if (dump_thbgm) {
				pfc::string8 filepath = basepath;
				filepath.add_string(bgm_path.c_str());
				pfc::string8 unpackdir = filepath;
				unpackdir.add_string("_unpack\\");
				if (!filesystem::g_exists(unpackdir, p_abort))
					filesystem::g_create_directory(unpackdir, p_abort);
				for (int i=1; i<bgmlist.size(); i++) {
					filepos = bgmlist[i]["filepos"];
					name_pos = filepos.find(',');
					pos_size = filepos.rfind(',');
					pfc::string8 outfile = unpackdir;
					outfile.add_string(filepos.substr(0, name_pos).c_str());
					internaloffset = _atoi64(filepos.substr(++name_pos, pos_size).c_str());
					internalsize = _atoi64(filepos.substr(++pos_size).c_str());

					service_ptr_t<file> m_file, p_out;
					filesystem::g_open(m_file, filepath, filesystem::open_mode_read, p_abort);
					filesystem::g_open_write_new(p_out, outfile, p_abort);
					pfc::array_t<char> buffer;
					buffer.set_size(internalsize);
					m_file->seek(internaloffset, p_abort);
					m_file->read(buffer.get_ptr(), internalsize, p_abort);
					p_out->write(buffer.get_ptr(), internalsize, p_abort);
				}
				dump_thbgm = false;
			}
			filepos = bgmlist[p_subsong]["filepos"];
			name_pos = filepos.find(',');
			pos_size = filepos.rfind(',');
			bgm_path.append("|");
			bgm_path.append(filepos.substr(0, name_pos));
			internaloffset = _atoi64(filepos.substr(++name_pos, pos_size).c_str());
			internalsize = _atoi64(filepos.substr(++pos_size).c_str());
		}
		if (isArchive) {
			t_uint32 fl = basepath.length() + bgm_path.find_first_of('|');
			char flstr[4];
			_itoa_s(fl, flstr, 10);
			path = "unpack://";
			path.append(pack);
			path.append("|");
			path.append(flstr);
			path.append("|");
			path.append(basepath);
		} else {
			path = basepath;
		}
		path.append(bgm_path);
		raw = service_impl_t<input_raw>();
		raw.open(path.c_str(), input_open_decode, isWave, p_abort);
	}

public:
	void open(file::ptr p_filehint, const char *p_path,
				t_input_open_reason p_reason, abort_callback &p_abort) {
		if (p_reason == input_open_info_write) {
			throw exception_io_unsupported_format();
		}
		m_file = p_filehint;
		input_open_file_helper(m_file, p_path, p_reason, p_abort);
		
		const char *pathlen = strrchr(p_path, '\\');
		int baselen = pathlen - p_path + 1;
		basepath.set_string(p_path, baselen);
		t_filesize length = m_file->get_size(p_abort);
		thxmlparser parser = thxmlparser();
		parser.parsestream( (char*) (m_file->read_string_ex(length, p_abort).get_ptr()));
		bgmlist = parser.thbgm;
		filelist = parser.filelist;
		pack = bgmlist[0]["pack"];
		if (bgmlist[1]["filepos"] != "") {
			pack = "raw";
			isStream = true;
		} else {
			isStream = false;
		}
		isArchive = pack != "";
		isWave = bgmlist[1]["codec"] == "PCM" && !isArchive;
	}

	t_uint32 get_subsong_count() {
		t_filesize count = bgmlist.size() - 1;
		totaltracks = pfc::format_int(count);
		return count;
	}

	t_uint32 get_subsong(t_uint32 p_subsong) {
		codec = bgmlist[++p_subsong]["codec"].c_str();
		encoding = bgmlist[p_subsong]["encoding"].c_str();
		title = bgmlist[p_subsong]["title"].c_str();
		artist = bgmlist[p_subsong]["artist"].c_str();

		string pos = bgmlist[p_subsong]["pos"];
		size_t pos_head = pos.find(',');
		size_t head_loop = pos.rfind(',');
		m_offset = _atoi64(pos.substr(0, pos_head).c_str());
		m_headlen = _atoi64(pos.substr(++pos_head, head_loop).c_str());
		m_looplen = _atoi64(pos.substr(++head_loop).c_str());
		m_totallen = m_headlen + m_looplen;

		samplerate = atoi(bgmlist[p_subsong]["samplerate"].c_str());
		bits = atoi(bgmlist[p_subsong]["bits"].c_str());
		channels = atoi(bgmlist[p_subsong]["channels"].c_str());
		samplewidth = bits * channels / 8;
		return p_subsong;
	}

	void get_info(t_uint32 p_subsong, file_info &p_info, abort_callback &p_abort) {
		p_info.info_set_int("samplerate", samplerate);
		p_info.info_set_int("channels", channels);
		p_info.info_set_int("bitspersample", bits);
		p_info.info_set("encoding", encoding);
		p_info.info_set("codec", codec);
		if (isWave) {
			p_info.info_set_bitrate((bits * channels * samplerate + 500) / 1000);
			p_info.set_length(audio_math::samples_to_time(
				length_to_samples(m_headlen + m_looplen), samplerate));
		} else {
			p_info.set_length(audio_math::samples_to_time(
				m_headlen + m_looplen, samplerate));
			if (read_thbgm_info) {
				open_raw(p_subsong, p_abort);
				raw.get_info(p_info, p_abort);
			}
		}

		p_info.meta_set("album", bgmlist[0]["album"].c_str());
		p_info.meta_set("album artist", bgmlist[0]["albumartist"].c_str());
		p_info.meta_set("title", bgmlist[p_subsong]["title"].c_str());
		p_info.meta_set("artist", bgmlist[p_subsong]["artist"].c_str());
		p_info.meta_set("tracknumber", pfc::format_int(p_subsong));
		p_info.meta_set("totaltracks", totaltracks);
	}

	t_filestats get_file_stats(abort_callback & p_abort) {
		return m_file->get_stats(p_abort);
	}

	void decode_initialize(t_uint32 p_subsong, unsigned p_flags,
				abort_callback &p_abort) {
		open_raw(get_subsong(--p_subsong), p_abort);
		if (isWave) {
			m_buffer.set_size(samples_to_length(deltaread));
		}
		current_loop = 1;
		decode_seek(0, p_abort);
	}

	bool decode_run(audio_chunk &p_chunk, abort_callback &p_abort) {
		if (isWave) {
			if (m_filepos >= m_maxseeksize) return false; 
			t_size deltaread_size = pfc::downcast_guarded<t_size>
				(pfc::min_t(samples_to_length(deltaread), m_maxseeksize - m_filepos));
			t_size deltaread_done = 
				raw.read(m_buffer.get_ptr(), deltaread_size, p_abort);
			m_filepos += deltaread_done;
		
			if (needloop && m_filepos >= m_maxseeksize) {
				t_filesize remain = samples_to_length(deltaread) - deltaread_done;
				m_filepos = m_headlen + remain;
				raw.raw_seek(m_offset + m_headlen, p_abort);
				deltaread_done += raw.read(m_buffer.get_ptr()
					+ deltaread_done, remain, p_abort);
				if (!loopforever) current_loop++;
			}

			p_chunk.set_data_fixedpoint(m_buffer.get_ptr(), deltaread_done,
				samplerate, channels, bits,
				audio_chunk::g_guess_channel_config(channels));
			return true;
		} else {
			return raw.run(p_chunk, p_abort);
		}
	}

	void decode_seek(double p_seconds, abort_callback &p_abort) {
		if (isWave) {
			t_uint64 samples = audio_math::time_to_samples(p_seconds, samplerate);
			m_filepos = samples * samplewidth;
			m_maxseeksize = m_headlen + m_looplen;
			if (m_filepos > m_maxseeksize)
				m_filepos = m_maxseeksize;
			raw.raw_seek(m_offset + m_filepos, p_abort);
		} else {
			raw.seek(p_seconds);
		}
	}

	bool decode_can_seek() {return true;}
	bool decode_get_dynamic_info_track(file_info &p_out,
			double &p_timestamp_delta) {return false;}
	bool decode_get_dynamic_info(file_info &p_info,
			double &p_timestamp_delta) {return false;}
	void decode_on_idle(abort_callback &p_abort) {}
	void retag_set_info(t_uint32 p_subsong, const file_info &p_info,
			abort_callback &p_abort) {}
	void retag_commit(abort_callback &p_abort) {}

	static bool g_is_our_content_type(const char *p_content_type) {return false;}
	static bool g_is_our_path(const char *p_path, const char *p_extension) {
		return stricmp_utf8(p_extension, "thxml") == 0;
	}
};
static input_factory_t<input_thxml> g_input_thbgm_factory;

DECLARE_FILE_TYPE("Touhou-like BGM XML-Tag File", "*.thxml");
DECLARE_COMPONENT_VERSION("ThBGM Player", "3.0", 
"Play BGM files of Touhou and some related doujin games.\n\n"
"If you have any feature request and bug report,\n"
"feel free to post an issue in the Github project.\n\n"
"https://github.com/nyfair/foo_thbgm/issues\n"
"(C) nyfair <nyfair2012@gmail.com>");
