#include <jsonutils.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <memory>

std::ostream& operator<<(std::ostream& out,const json::Pretty& p) {
	if (p.ptr)
		p.ptr->prettyPrint(out, 0);
	return out;
}

namespace json {

const std::string NULL_STRING="null";
const std::string TRUE_STRING="true";
const std::string FALSE_STRING="false";
jnullptr JSON_NULL_INSTANCE=std::make_shared<JsonNull>();
jboolptr JSON_TRUE_INSTANCE=std::make_shared<JsonTrue>();
jboolptr JSON_FALSE_INSTANCE=std::make_shared<JsonFalse>();


const char* jsonTypeString(JsonType t) {
	switch (t) {
		case JNULL: return "JNULL";
		case JOBJECT: return "JOBJECT";
		case JARRAY: return "JARRAY";
		case JSTRING: return "JSTRING";
		case JNUMBER: return "JNUMBER";
		case JBOOLEAN: return "JBOOLEAN";
	}
	return "not implemented";
}
// based on https://github.com/sheredom/utf8.h
inline size_t utf8CodepointSize(const char *str) noexcept {
  if (0xf0 == (0xf8 & str[0])) {
    /* 4 byte utf8 codepoint */
    return 4;
  } else if (0xe0 == (0xf0 & str[0])) {
    /* 3 byte utf8 codepoint */
    return 3;
  } else if (0xc0 == (0xe0 & str[0])) {
    /* 2 byte utf8 codepoint */
    return 2;
  }
  /* 1 byte utf8 codepoint otherwise */
  return 1;
}
inline char *utf8CatCodepoint(char *str, uint32_t chr, size_t n) {
  if (0 == ((uint32_t)0xffffff80 & chr)) {
    /* 1-byte/7-bit ascii
     * (0b0xxxxxxx) */
    if (n < 1) {
      return NULL;
    }
    str[0] = (char)chr;
    str += 1;
  } else if (0 == ((uint32_t)0xfffff800 & chr)) {
    /* 2-byte/11-bit utf8 code point
     * (0b110xxxxx 0b10xxxxxx) */
    if (n < 2) {
      return NULL;
    }
    str[0] = (char)(0xc0 | (char)((chr >> 6) & 0x1f));
    str[1] = (char)(0x80 | (char)(chr & 0x3f));
    str += 2;
  } else if (0 == ((uint32_t)0xffff0000 & chr)) {
    /* 3-byte/16-bit utf8 code point
     * (0b1110xxxx 0b10xxxxxx 0b10xxxxxx) */
    if (n < 3) {
      return NULL;
    }
    str[0] = (char)(0xe0 | (char)((chr >> 12) & 0x0f));
    str[1] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
    str[2] = (char)(0x80 | (char)(chr & 0x3f));
    str += 3;
  } else {
    if (n < 4) {
      return NULL;
    }
    str[0] = (char)(0xf0 | (char)((chr >> 18) & 0x07));
    str[1] = (char)(0x80 | (char)((chr >> 12) & 0x3f));
    str[2] = (char)(0x80 | (char)((chr >> 6) & 0x3f));
    str[3] = (char)(0x80 | (char)(chr & 0x3f));
    str += 4;
  }
  return str;
}
inline void utf8CatCodepoint(std::string& str, uint32_t ch) {
  if (0 == ((uint32_t)0xffffff80 & ch)) {
    /* 1-byte/7-bit ascii
     * (0b0xxxxxxx) */
    str+=(char)ch;
  } else if (0 == ((uint32_t)0xfffff800 & ch)) {
    /* 2-byte/11-bit utf8 code point
     * (0b110xxxxx 0b10xxxxxx) */
    str += (char)(0xc0 | (char)((ch >> 6) & 0x1f));
    str += (char)(0x80 | (char)(ch & 0x3f));
  } else if (0 == ((uint32_t)0xffff0000 & ch)) {
    /* 3-byte/16-bit utf8 code point
     * (0b1110xxxx 0b10xxxxxx 0b10xxxxxx) */
    str += (char)(0xe0 | (char)((ch >> 12) & 0x0f));
    str += (char)(0x80 | (char)((ch >> 6) & 0x3f));
    str += (char)(0x80 | (char)(ch & 0x3f));
    str += 3;
  } else {

    str += (char)(0xf0 | (char)((ch >> 18) & 0x07));
    str += (char)(0x80 | (char)((ch >> 12) & 0x3f));
    str += (char)(0x80 | (char)((ch >> 6) & 0x3f));
    str += (char)(0x80 | (char)(ch & 0x3f));
  }
}
void escapeTo(std::string& dst, const char* ptr) {
	while (*ptr) {
		size_t codepointSize=utf8CodepointSize(ptr);
		if (codepointSize==1) {
			char ch=*ptr;
			switch (ch) {
			case '\b': dst+="\\b"; break;
			case '\f': dst+="\\f"; break;
			case '\n': dst+="\\n"; break;
			case '\r': dst+="\\r"; break;
			case '\t': dst+="\\t"; break;
			case '"':  dst+="\\\""; break;
			case '\\': dst+="\\\\"; break;
			case '/': dst+="\\/"; break;
			default:
				dst+=ch;
			}
			++ptr;
		} else {
			auto nextPos=ptr+codepointSize;
			dst.append(ptr,nextPos);
			ptr=nextPos;
			/* could it be?
			while (ptr!=nextPos) {
				char ch=*ptr;
				if (ch==0) goto END;
				dst+=ch;
				++ptr;
			}*/
		}
	}
	//END:
}
void escapeTo(std::ostream& dst, const char* ptr) {
	while (*ptr) {
		size_t codepointSize=utf8CodepointSize(ptr);
		if (codepointSize==1) {
			char ch=*ptr;
			switch (ch) {
			case '\b': dst<<"\\b"; break;
			case '\f': dst<<"\\f"; break;
			case '\n': dst<<"\\n"; break;
			case '\r': dst<<"\\r"; break;
			case '\t': dst<<"\\t"; break;
			case '"':  dst<<"\\\""; break;
			case '\\': dst<<"\\\\"; break;
			case '/': dst<<"\\/"; break;
			default:
				dst<<ch;
			}
			++ptr;
		} else {
			auto nextPos=ptr+codepointSize;
			while (ptr!=nextPos) {
				char ch=*ptr;
				dst<<ch;
				++ptr;
			}
		}
	}
	//END:
}
void JsonString::appendToString(std::string& dst) const {
	dst+='"';
	const char* ptr=raw.c_str();
	escapeTo(dst,ptr);
	dst+='"';
}

void JsonObject::appendToString(std::string& dst) const {
	dst+='{';
	json::JsonObjectIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) dst+=',';
		auto pair=*it;
		auto& key=pair.first;
		dst+='"';
		escapeTo(dst,key.c_str());
		dst+='"';
		dst+=':';
		auto& obj=pair.second;
		obj->appendToString(dst);
		++it;
		needComma=true;
	}
	dst+='}';
}
void JsonArray::appendToString(std::string& dst) const {
	dst+='[';
	json::JsonArrayIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) dst+=',';
		auto el=*it;
		el->appendToString(dst);
		++it;
		needComma=true;
	}
	dst+=']';
}

void JsonArray::print(std::ostream& dst) const {
	dst<<'[';
	json::JsonArrayIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) dst<<',';
		auto el=*it;
		el->print(dst);
		++it;
		needComma=true;
	}
	dst<<']';
}
void JsonObject::print(std::ostream& dst) const {
	dst<<'{';
	json::JsonObjectIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) dst<<',';
		auto pair=*it;
		auto& key=pair.first;
		dst<<'"';
		escapeTo(dst,key.c_str());
		dst<<'"';
		dst<<':';
		auto& obj=pair.second;
		obj->print(dst);
		++it;
		needComma=true;
	}
	dst<<'}';
}
void JsonString::print(std::ostream& dst) const {
	dst<<'"';
	const char* ptr=raw.c_str();
	escapeTo(dst,ptr);
	dst<<'"';
}

void JsonString::prettyPrint(std::ostream& out, int off, int policy) const {
	print(out);
}
void JsonNull::prettyPrint(std::ostream& out, int off, int policy) const {
	print(out);
}
void JsonBoolean::prettyPrint(std::ostream& out, int off, int policy) const {
	print(out);
}
void JsonNumber::prettyPrint(std::ostream& out, int off, int policy) const {
	print(out);
}
void JsonObject::prettyPrint(std::ostream& dst, int off, int policy) const {
	if (memberMap.empty()) {
		dst<<"{}";
		return;
	}
	if (simple()) {
		dst<<"{ ";
		JsonObjectIterator it(this);
		auto pair=*it;
		auto& key=pair.first;
		auto& v=pair.second;
		dst<<'"';
		escapeTo(dst,key.c_str());
		dst<<'"';
		dst<<" : ";
		v->print(dst);
		dst<<" }";
		return;
	}
	dst<<"{\n";
	JsonObjectIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) {
			dst<<',';
			dst<<'\n';
		}
		auto pair=*it;
		auto& key=pair.first;
		for (int i=0;i<off+1;++i) dst<<'\t';
		dst<<'"';
		escapeTo(dst,key.c_str());
		dst<<'"';
		dst<<" : ";
		auto& obj=pair.second;
		obj->prettyPrint(dst, off+1, policy);
		++it;
		needComma=true;
	}
	if (needComma) dst<<'\n';
	for (int i=0;i<off;++i) dst<<'\t';
	dst<<'}';
}
bool JsonObject::simple() const {
	if (memberMap.begin()==memberMap.end()) return true;
	if (std::next(memberMap.begin())==memberMap.end()) {
		auto& el=std::get<0>(memberMap.begin()->second);
		if (el->isJsonPrimitive()) return true;
		if (el->isJsonArray() && el->getAsJsonArray()->simple()) return true;
		if (el->isJsonObject() && el->getAsJsonObject()->simple()) return true;
	}
	return false;
}
bool JsonArray::simple() const {
	if (elements.begin()==elements.end()) return true;
	if (std::next(elements.begin())==elements.end()) {
		auto& el=*elements.begin();
		if (el->isJsonPrimitive()) return true;
		if (el->isJsonArray() && el->getAsJsonArray()->simple()) return true;
		if (el->isJsonObject() && el->getAsJsonObject()->simple()) return true;
	}
	return false;
}

void JsonArray::prettyPrint(std::ostream& dst, int off, int policy) const {
	if (empty()) {
		dst<<"[]";
		return;
	} else if (simple()) {
		dst<<"[ ";
		(*elements.begin())->print(dst);
		dst<<" ]";
		return;
	}

	dst<<"[\n";
	json::JsonArrayIterator it(this);
	bool needComma=false;
	while (it) {
		if (needComma) {
			dst<<',';
			dst<<'\n';
		}
		for (int i=0;i<off+1;++i) dst<<'\t';
		const auto& el=*it;
		el->prettyPrint(dst, off+1, policy);
		++it;
		needComma=true;
	}
	if (needComma) dst<<'\n';
	for (int i=0;i<off;++i) dst<<'\t';
	dst<<']';
}


void print(std::ostream& out,const JsonElement* ptr) {
	if (ptr) {
		ptr->print(out);
	}
}

struct MemWindow {
	const char* pos;
	const char* endPos;
	MemWindow(const char* p,const char* e):pos(p),endPos(e) {}
	bool available() noexcept {return pos<endPos;}
	bool exhausted() noexcept {return pos>=endPos;}
	bool skipCurrentBlanks() noexcept {
		for (;;) {
			if (exhausted()) return false;
			char c=*pos;
			if (c==' ' || c=='\t' || c=='\r' || c=='\n' || c=='\f') ++pos;
			else return true;
		}
		return true;
	}
	char current() noexcept {return *pos;}
	char consume() noexcept {return *pos++;}
	size_t size() noexcept {return endPos-pos;}
	const char* position() noexcept {return pos;}
	const char* end() noexcept {return endPos;}
	void advance(size_t n) noexcept {pos+=n;}
	void setPosition(const char* p) noexcept {pos=p;}
};

struct MemParser {
	MemWindow window;
	MemParser(const char* p,const char* e) : window(p,e) {}
	const char* errorPos{nullptr};
	std::string errorMessage;
	bool consumeOpenedJsonString(std::string& str);
	jobjptr parseOpenedObject();
	jarrptr parseOpenedArray();
	void unexpectedEnd() {errorPos=window.position();errorMessage="Unexpected end";}
	void error(const std::string& str) {errorPos=window.position();errorMessage=str;}
	void errorRollback(const std::string& str, size_t i) {window.pos-=i;errorPos=window.position();errorMessage=str;}
	bool parserError() noexcept {return errorPos!=nullptr;}
	const std::string getErrorMessage() {
		if (errorPos!=nullptr && errorPos<window.end()) return errorMessage+" at '"+std::string(errorPos, window.end())+"'";
		return errorMessage;
	}
	jptr parseArrayOrObject();
};

bool testConsumeOpenedString(std::string& str, const char* s) {
	MemParser p(s, s+::strlen(s));
	p.window.pos=s;
	p.window.endPos=s+::strlen(s);
	return p.consumeOpenedJsonString(str);
}

bool MemParser::consumeOpenedJsonString(std::string& str) {
	bool escapeMode=false;
	for (;;) {
		auto cpSize=utf8CodepointSize(window.position());
		if (cpSize>window.size()) {
			unexpectedEnd();
			return false;
		}
		if (escapeMode) {
			if (cpSize>1) {
				error("unexpected multi-byte character after escape");
				return false;
			}
			char ch=window.consume();
			switch (ch) {
				case '"':
				case '\\':
				case '/':
					str+=ch;
					break;
				case 'b': str+='\b'; break;
				case 'f': str+='\f'; break;
				case 'n': str+='\n'; break;
				case 'r': str+='\r'; break;
				case 't': str+='\t'; break;
				case 'u': {
					if (window.size()<4) {
						unexpectedEnd();
						return false;
					}
					std::string s(window.position(),window.position()+4);
					uint32_t cp;
					try {cp=std::stoul(s,nullptr,16);} catch(...) {
						errorRollback("invalid unicode \""+s+"\"",1);
						return false;
					}
					window.advance(4);
					if (cp <= 0xD7FF || cp >= 0xE000) {// normal
						utf8CatCodepoint(str,cp);
					} else if (cp >= 0xd800 && cp <= 0xdbff) { // high surrogate
						uint32_t hs=cp;
						if (window.size()<6) {
							unexpectedEnd();
							return false;
						}
						// expect \uLLLL
						if (*window.position()!='\\' || *(window.position()+1)!='u') {
							error("invalid unicode sequence");
							return false;
						}
						std::string low(window.position()+2,window.position()+6);
						uint32_t ls;
						try {ls=std::stoul(low,nullptr,16);} catch(...){
							error("invalid unicode low surrogate \""+low+"\"");
							return false;
						}
						if (ls< 0xDC00 || ls > 0xDFFF) {
							error("invalid unicode low surrogate \""+low+"\"");
							return false;
						}
						cp=((hs-0xD800)*0x400 + (ls-0xDC00)) +0x10000;
						window.advance(6);
						utf8CatCodepoint(str,cp);
					} else {
						errorRollback("invalid unicode \""+s+"\"",1);
						return false;
					}//
				}
				break;
				default:
					error("unknown escape character");
					return false;
			}
			escapeMode=false;
		} else {
			if (cpSize==1) {
				char ch=window.consume();
				if (ch=='"') return true;
				else if (ch=='\\') {
					if (window.exhausted()) {
						unexpectedEnd();
						return false;
					}
					escapeMode=true;
					continue;
				} else {
					str+=ch;
				}
			} else {
				auto oldPos=window.position();
				window.advance(cpSize);
				str.append(oldPos, window.position());
			}
		}
	}
	return false; // never the case
}

jobjptr MemParser::parseOpenedObject() {
	auto obj=makeJsonObject();
	bool expectComma=false;
	for (;;) {
		if (window.skipCurrentBlanks()) {
			char ch=window.consume();
			if (ch=='}') return obj;
			if (expectComma) {
				expectComma=false;
				if (ch==',') {
					if (window.skipCurrentBlanks()) ch=window.consume();
					else {
						unexpectedEnd();
						return obj;
					}
				} else {
					errorRollback("Expected \",\" or \"}\"",1);
					return obj;
				}
			}
			if (ch=='"') {
				// reading attribute name
				std::string attr;
				if (consumeOpenedJsonString(attr)) {
					if (window.skipCurrentBlanks()) {
						ch=window.consume();
						if (ch==':') {
							//got to the value
							if (window.skipCurrentBlanks()) {
								ch=window.consume();
								if (ch=='"') { // its a string
									std::string val;
									if (consumeOpenedJsonString(val)) obj->add(std::move(attr), std::move(val));
									else return obj; // consumeOpenedJsonString provides error
								} else if (ch=='{') { // its an object
									auto child=parseOpenedObject();
									if (parserError()) return obj;
									obj->addNotEmptyChild(std::move(attr), std::move(child));
								} else if (ch=='[') { // its an array
									auto child=parseOpenedArray();
									if (parserError()) return obj;
									obj->addNotEmptyChild(std::move(attr), std::move(child));
								} else if (ch=='n') { // null or error
									if (window.size()>=3 && ::strncmp(window.position(), "ull", 3)==0) {
										window.advance(3);
										obj->addNotEmptyChild(std::move(attr), makeJsonNull());
									} else {
										errorRollback("Expected \"null\"", 1);
										return obj;
									}
								} else if (ch=='f') { // false or error
									if (window.size()>=4 && ::strncmp(window.position(), "alse", 4)==0) {
										window.advance(4);
										obj->addNotEmptyChild(std::move(attr), JSON_FALSE_INSTANCE);
									} else {
										errorRollback("Expected \"false\"", 1);
										return obj;
									}
								} else if (ch=='t') { // true or error
									if (window.size()>=3 && ::strncmp(window.position(), "rue", 3)==0) {
										window.advance(3);
										obj->addNotEmptyChild(std::move(attr), JSON_TRUE_INSTANCE);
									} else {
										errorRollback("Expected \"true\"", 1);
										return obj;
									}
								} else if ( (ch>='0' && ch<='9') || ch=='-' || ch=='+' || ch=='.') { // number
									auto p=window.position();
									for (;;) {
										if (window.size()==0) {
											unexpectedEnd();
											return obj;
										}
										ch=*p;
										if ( !(ch>='0' && ch<='9') && ch!='-' && ch!='+' && ch!='.' && ch!='e' && ch!='E') {
											std::string val(window.position()-1,p);
											obj->addNotEmptyChild(std::move(attr), makeJsonNumber(std::move(val)));
											window.setPosition(p);
											break;
										} else ++p;
									}
								} else {
									errorRollback("unexpected character",1);
									return obj;
								}
								// finished constructing property
								expectComma=true;
								continue;
							} else {
								unexpectedEnd();
								return obj;
							}
						} else {
							errorRollback("expected \":\"",1);
							return obj;
						}
					} else {
						unexpectedEnd();
						return obj;
					}
				} else {
					// consumeOpenedJsonString failed
					return obj;
				}
			} // done reading attribute/value pair
			else {
				errorRollback("expected '\"'",1);
				return obj;
			}
		} else {
			unexpectedEnd();
			return obj;
		}
	}
	return obj;
}


jarrptr MemParser::parseOpenedArray() {
	auto arr=makeJsonArray();
	bool expectComma=false;
	for (;;) {
		if (window.skipCurrentBlanks()) {
			char ch=window.consume();
			if (ch==']') return arr;
			if (expectComma) {
				expectComma=false;
				if (ch==',') {
					if (window.skipCurrentBlanks()) ch=window.consume();
					else {
						unexpectedEnd();
						return arr;
					}
				} else {
					errorRollback("Expected \",\" or \"]\"",1);
					return arr;
				}
			}
			if (ch=='"') { // reading string value
				std::string val;
				if (consumeOpenedJsonString(val)) arr->add(std::move(val));
				else return arr;
			} else if (ch=='{') { // its an object
				auto child=parseOpenedObject();
				if (parserError()) return arr;
				arr->addNotEmpty(std::move(child));
			} else if (ch=='[') { // its an array
				auto child=parseOpenedArray();
				if (parserError()) return arr;
				arr->addNotEmpty(std::move(child));
			} else if (ch=='n') { // null or error
				if (window.size()>=3 && ::strncmp(window.position(), "ull", 3)==0) {
					window.advance(3);
					arr->addNotEmpty(makeJsonNull());
				} else {
					errorRollback("Expected \"null\"", 1);
					return arr;
				}
			} else if (ch=='f') { // false or error
				if (window.size()>=4 && ::strncmp(window.position(), "alse", 4)==0) {
					window.advance(4);
					arr->addNotEmpty(JSON_FALSE_INSTANCE);
				} else {
					errorRollback("Expected \"false\"", 1);
					return arr;
				}
			} else if (ch=='t') { // true or error
				if (window.size()>=3 && ::strncmp(window.position(), "rue", 3)==0) {
					window.advance(3);
					arr->addNotEmpty(JSON_TRUE_INSTANCE);
				} else {
					errorRollback("Expected \"true\"", 1);
					return arr;
				}
			} else if ( (ch>='0' && ch<='9') || ch=='-' || ch=='+' || ch=='.') { // number
				auto p=window.position();
				for (;;) {
					if (window.size()==0) {
						unexpectedEnd();
						return arr;
					}
					ch=*p;
					if ( !(ch>='0' && ch<='9') && ch!='-' && ch!='+' && ch!='.' && ch!='e' && ch!='E') {
						std::string val(window.position()-1,p);
						arr->addNotEmpty(makeJsonNumber(std::move(val)));
						window.setPosition(p);
						break;
					} else ++p;
				}
			} else {
				errorRollback("unexpected character",1);
				return arr;
			}
			// finished constructing array element
			expectComma=true;
			continue;
		} else {
			unexpectedEnd();
			return arr;
		}
	}
	return arr;
}

jptr MemParser::parseArrayOrObject() {
	if (window.skipCurrentBlanks()) {
		char ch=window.consume();
		if (ch=='{') return parseOpenedObject();
		else if (ch=='[') return parseOpenedArray();
		else {
			errorRollback("Not a json object or array",1);
		}
	} return jptr();
}

jptr parseArrayOrObject(const char* start, const char* end, size_t* consumedBytesPtr) {
	MemParser ps(start,end);
	jptr ret=ps.parseArrayOrObject();
	if (ps.parserError()) throw std::runtime_error(ps.getErrorMessage());
	if (consumedBytesPtr) *consumedBytesPtr=ps.window.position()-start;
	return ret;
}

bool isRegularFile(const char* name, ssize_t* ps) {
	struct stat st;
	int sr=::stat(name, &st);
	if (sr==0) {
		if (ps) *ps=st.st_size;
		return (st.st_mode & S_IFMT)==S_IFREG;
	}
	if (ps) *ps=-1;
	return false;
}

struct FD {
	int fd;
	FD() : fd(-1) {}
	FD(int _fd) : fd(_fd) {}
	FD(const FD & ) = delete;
	FD(FD && ) = delete;
	FD& operator =(const FD & ) =delete;
	FD& operator =(FD && ) =delete;
	operator bool() {return fd>=0;}
	operator int() {return fd;}
	FD& operator =(int v) {
		if (fd>=0) ::close(fd);
		fd=v;
		return *this;
	}
	~FD() {if (fd>=0) ::close(fd);}
};
std::string errno_string() {
	auto z=strerror_l(errno, uselocale((locale_t)0));
	return z?std::string(z):"";
}
void errno_string(std::string& str) {
	str=errno_string();
}

void errno_exception(const std::string& msg) {
	auto e=errno_string();
	throw std::runtime_error(msg+": "+e);
}

jptr parseFromFile(const char* fileName, size_t pos, size_t* bytesRead) {
	if (!fileName) throw std::runtime_error("fileName can't be null in json::parseFromFile");
	ssize_t size;
	if (isRegularFile(fileName, &size)) {
		FD fd=::open(fileName,O_RDONLY | O_NOATIME);
		if (!fd) errno_exception("Failed to open "+std::string(fileName));
		auto unmap=[&size](void* map) {
			if (map>0) ::munmap(map, size);
		};
		std::unique_ptr<void, decltype(unmap)> map(::mmap(0, size, PROT_READ, MAP_SHARED, (int)fd, 0),unmap);
		if (map.get()==MAP_FAILED) errno_exception("Failed to mmap "+std::string(fileName));
		char* start=((char*)map.get())+pos;
		char* end=((char*)map.get())+size-pos;
		return parseArrayOrObject(start,end,bytesRead);
	} else {
		throw std::runtime_error(std::string(fileName)+" file is missing, or not a regular file in json::parseFromFile. Not yet supported.");
	}
	return jptr();
}

}



