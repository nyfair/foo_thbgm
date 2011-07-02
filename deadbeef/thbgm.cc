#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <map>
#include <string>
#include <vector>

using namespace std;
#include "../thxmlparser.h"
#include <deadbeef/deadbeef.h>

#define trace(fmt,...)

static DB_decoder_t plugin;
static DB_functions_t *deadbeef;

typedef struct {
	DB_fileinfo_t info;
	DB_FILE *file;
	uint64_t m_offset;
	uint64_t m_headlen;
	uint64_t m_looplen;
	uint64_t m_totallen;
	uint64_t m_current;
	const char *bgmpath;
} thbgm_info;

static const char *ext[] = {"thxml", NULL};

static const char settings_dlg[] =
    "property \"thbgm loop forever\" checkbox thbgm.loop 1;"
    "property \"read thbgm fileinfo\" checkbox thbgm.readinfo 0;"
;

bool loopforever = true;
bool read_thbgm_info = false;

const int deltaread = 1024;
string pack;
bool isWave;
bool isArchive;

static DB_fileinfo_t *thbgm_open(uint32_t hints) {
	DB_fileinfo_t *_info = (DB_fileinfo_t*) malloc(sizeof(thbgm_info));
	memset(_info, 0, sizeof(thbgm_info));
	return _info;
}

static int thbgm_init(DB_fileinfo_t *_info, DB_playItem_t *it) {
	trace("thbgm_init");
	thbgm_info *info = (thbgm_info *)_info;
	info->file = deadbeef->fopen(info->bgmpath);
	return 0;
}

static void thbgm_free(DB_fileinfo_t *_info) {
	trace("thbgm_free");
	thbgm_info *info = (thbgm_info *)_info;
	if(info) free(info);
}

static int thbgm_read(DB_fileinfo_t *_info, char *bytes, int size) {
	trace("thbgm_read");
	thbgm_info *info = (thbgm_info *)_info;
	deadbeef->fread(bytes, 1, 1024, info->file);
	return 1024;
}

static int thbgm_seek_sample(DB_fileinfo_t *_info, int sample) {
	trace("thbgm_ss");
	thbgm_info *info = (thbgm_info *)_info;
	_info->readpos = (float) sample / _info->fmt.samplerate + info->m_offset;
	return 0;
}

static int thbgm_seek(DB_fileinfo_t *_info, float time) {
	trace("thbgm_s");
	return thbgm_seek_sample(_info, time * _info->fmt.samplerate);
}

static DB_playItem_t *thbgm_insert(ddb_playlist_t *plt,
				DB_playItem_t *after, const char *fname) {
	thxmlparser parser = thxmlparser();
	parser.parsefile(fname);
	vector<map<string, string> > bgmlist = parser.thbgm;
	
	int listsize = bgmlist.size();
	for(int i=1; i<listsize; i++) {
		thbgm_info info;
		memset(&info, 0, sizeof(info));
		DB_fileinfo_t _info = info.info;

		DB_playItem_t *it = deadbeef->pl_item_alloc_init(fname, plugin.plugin.id);
		deadbeef->pl_add_meta(it, "title", bgmlist[i]["title"].c_str());
		deadbeef->pl_add_meta(it, "artist", bgmlist[i]["artist"].c_str());
		deadbeef->pl_add_meta(it, "album", bgmlist[0]["album"].c_str());
		deadbeef->pl_add_meta(it, "band", bgmlist[0]["albumartist"].c_str());
		deadbeef->pl_set_meta_int(it, "track", i);
		deadbeef->pl_set_meta_int(it, "numtracks", listsize);

		_info.fmt.samplerate = atoi(bgmlist[i]["samplerate"].c_str());
		_info.fmt.channels = atoi(bgmlist[i]["channels"].c_str());
		_info.fmt.bps = atoi(bgmlist[i]["bits"].c_str());
		for(int j=0; j<_info.fmt.channels; j++) {
				_info.fmt.channelmask |= 1 << j;
		}
		
		info.bgmpath = bgmlist[i]["bgmpath"].c_str();
		string pos = bgmlist[i]["pos"];
		size_t pos_head = pos.find(',');
		size_t head_loop = pos.rfind(',');
		info.m_current = 0;
		info.m_offset = atol(pos.substr(0, pos_head).c_str());
		info.m_headlen = atol(pos.substr(++pos_head, head_loop).c_str());
		info.m_looplen = atol(pos.substr(++head_loop).c_str());
		info.m_totallen = info.m_headlen + info.m_looplen;
		deadbeef->plt_set_item_duration(plt, it,
							(double)info.m_totallen / 8 / _info.fmt.samplerate);
		
		after = deadbeef->plt_insert_item(plt, after, it);
		deadbeef->pl_item_unref(it);	
	}
	return after;
}

static int thbgm_message(uint32_t id, uintptr_t ctx, uint32_t p1, uint32_t p2) {
	switch(id) {
		case DB_EV_ACTIVATED:
		case DB_EV_CONFIGCHANGED:
			loopforever = deadbeef->conf_get_int("thbgm.readinfo", 1);
			read_thbgm_info = deadbeef->conf_get_int("thbgm.readinfo", 0);
			break;
	}
	return 0;
}

extern "C" { 
	DB_plugin_t *thbgm_load(DB_functions_t *api) {
		deadbeef = api;
		plugin.plugin.api_vmajor = 1;
		plugin.plugin.api_vminor = 0;
		plugin.plugin.version_major = 1;
		plugin.plugin.version_minor = 0;
		plugin.plugin.type = DB_PLUGIN_DECODER;
		plugin.plugin.id = "ZWAV";
		plugin.plugin.name = "ThBGM Player";
		plugin.plugin.descr = "Touhou Game Music Player";
		plugin.plugin.copyright = 
			"Play BGM files of Touhou and some related doujin games.\n\n \
			If you have any feature request and bug report,\n \
			feel free to contact me at my E-mail address below.\n\n \
			(C) nyfair <nyfair2012@gmail.com>";
		plugin.plugin.configdialog = settings_dlg;
		plugin.plugin.message = thbgm_message;
		plugin.open = thbgm_open;
		plugin.init = thbgm_init;
		plugin.free = thbgm_free;
		plugin.read = thbgm_read;
		plugin.seek = thbgm_seek;
		plugin.seek_sample = thbgm_seek_sample;
		plugin.insert = thbgm_insert;
		plugin.exts = ext;
		return DB_PLUGIN(&plugin);
	}
}
