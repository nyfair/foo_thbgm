#include <map>
#include <string>
#include <vector>
#include <rapidxml/rapidxml.hpp>
#include <rapidxml/rapidxml_utils.hpp>

using namespace std;
using namespace rapidxml;
#include "thxmlparser.h"

thxmlparser::thxmlparser() {
	map<string, string> bgminfo;
	bgminfo["samplerate"] = "44100";
	bgminfo["bits"] = "16";
	bgminfo["channels"] = "2";
	bgminfo["codec"] = "PCM";
	bgminfo["encoding"] = "lossless";
	bgminfo["artist"] = "ZUN";
	bgminfo["pack"] = "";
	thbgm.push_back(bgminfo);
}

void thxmlparser::parsefile(const char *thxml_file) {
	file<> thxml(thxml_file);
	parsestream(thxml.data());
}

void thxmlparser::parsestream(char *thxml_stream) {
	xml_document<> doc;
	doc.parse<parse_default>(thxml_stream);

	xml_node<> *root = doc.first_node();
	xml_node<> *album_node = root->first_node("album");
	thbgm[0]["album"] = album_node->value();

	for(xml_attribute<> *attr = album_node->first_attribute();
			attr; attr = attr->next_attribute()) {
		thbgm[0][attr->name()] = attr->value();
	}
	thbgm[0]["albumartist"] = root->first_node("albumartist")->value();
	thbgm[0]["path"] = root->first_node("path")->value();

	xml_node<> *bgmlist_node = root->first_node("bgmlist");
	for(xml_node<> *bgm_node = bgmlist_node->first_node();
			bgm_node; bgm_node = bgm_node->next_sibling()) {
		map<string, string> bgm = construct_basebgm();
		bgm["title"] = bgm_node->value();
		for(xml_attribute<> *attr = bgm_node->first_attribute();
				attr; attr = attr->next_attribute()) {
			bgm[attr->name()] = attr->value();
		}
		bgm["file"] = thbgm[0]["path"] + bgm["file"];
		thbgm.push_back(bgm);
	}
}

map<string, string> thxmlparser::construct_basebgm() {
	map<string, string> trackinfo;
	trackinfo["samplerate"] = thbgm[0]["samplerate"];
	trackinfo["bits"] = thbgm[0]["bits"];
	trackinfo["channels"] = thbgm[0]["channels"];
	trackinfo["codec"] = thbgm[0]["codec"];
	trackinfo["encoding"] = thbgm[0]["encoding"];
	trackinfo["artist"] = thbgm[0]["artist"];
	return trackinfo;
}
