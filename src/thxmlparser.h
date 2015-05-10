#ifndef THXML_PARSER_H
#define THXML_PARSER_H

class thxmlparser {
public:
	vector<map<string, string>> thbgm;
	vector<map<string, string>> filelist;
	thxmlparser();
	virtual void parsefile(const char*);
	virtual void parsestream(char*);
private:
	virtual map<string, string> construct_basebgm();
};
#endif
