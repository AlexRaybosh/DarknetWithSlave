#ifndef JSONUTILS_H_
#define JSONUTILS_H_
#include <memory>
#include <string>
#include <list>
#include <vector>
#include <set>
#include <map>
//#include <unordered_map>
//#include <unordered_set>
#include <string.h>
#include <iostream>
#include <optional>
#include <utility>
#include <ostream>

namespace json {

class JsonElement;
class JsonPrimitive;
class JsonDataPrimitive;
class JsonNull;
class JsonBoolean;
class JsonTrue;
class JsonFalse;
class JsonString;
class JsonNumber;
class JsonObject;
class JsonArray;

typedef std::shared_ptr<JsonElement> jptr;
typedef std::shared_ptr<JsonString> jstrptr;
typedef std::shared_ptr<JsonNumber> jnumptr;
typedef std::shared_ptr<JsonBoolean> jboolptr;
typedef std::shared_ptr<JsonNull> jnullptr;
typedef std::shared_ptr<JsonObject> jobjptr;
typedef std::shared_ptr<JsonArray> jarrptr;

enum JsonType {
	JNULL,
	JOBJECT,
	JARRAY,
	JSTRING,
	JNUMBER,
	JBOOLEAN
};
const char* jsonTypeString(JsonType t);

jboolptr makeJsonBoolean(bool val);
jnullptr makeJsonNull();
jobjptr makeJsonObject();
jarrptr makeJsonArray();

class JsonElement: public std::enable_shared_from_this<JsonElement> {
public:
	virtual ~JsonElement()=default;
	virtual JsonType getJsonType() const = 0;
	virtual bool isJsonArray() const {return false;}
	virtual bool isJsonObject() const {return false;}
	virtual bool isJsonPrimitive() const {return false;}
	virtual bool isJsonNull() const {return false;}
	virtual bool isBoolean() const {return false;}
	virtual bool isNumber() const {return false;}
	virtual bool isString() const {return false;}
	virtual int64_t longValue() const {throw std::runtime_error("not a number");}
	virtual double doubleValue() const {throw std::runtime_error("not a number");}
	virtual bool booleanValue() const {throw std::runtime_error("not a boolean");}
	virtual const std::string& stringValue() const {throw std::runtime_error("not a string");}
	virtual std::string& rawStringValue() {throw std::runtime_error("not a data primitive");}
	virtual void appendToString(std::string& str) const =0;
	virtual void print(std::ostream& out) const =0;
	virtual void prettyPrint(std::ostream& out, int off=0, int policy=0) const =0;
	std::string toString() const {
		std::string ret;
		appendToString(ret);
		return ret;
	}
	jptr getAsJsonElement() const {
		return std::static_pointer_cast<JsonElement>(const_cast<JsonElement*>(this)->shared_from_this());
	}
	virtual jobjptr getAsJsonObject() const {return jobjptr();}
	virtual jarrptr getAsJsonArray() const {return jarrptr();}
	virtual jstrptr getAsJsonString() const {return jstrptr();}
	virtual jnumptr getAsJsonNumber() const {return jnumptr();}
	virtual jboolptr getAsJsonBoolean() const {return jboolptr();}
	virtual jnullptr getAsJsonNull() {return jnullptr();}
	friend class Pretty;
};
extern const std::string NULL_STRING;
extern const std::string TRUE_STRING;
extern const std::string FALSE_STRING;
extern jnullptr JSON_NULL_INSTANCE;
extern jboolptr JSON_TRUE_INSTANCE;
extern jboolptr JSON_FALSE_INSTANCE;

class JsonPrimitive: public JsonElement {
public:
	bool isJsonPrimitive() const {return true;}
};

class JsonNull: public JsonPrimitive {
public:
	bool isJsonNull() const {return true;}
	JsonType getJsonType() const {return JsonType::JNULL;};
	void appendToString(std::string& str) const {str+=NULL_STRING;}
	void print(std::ostream& out) const {out<<NULL_STRING;}
	const std::string& stringValue() const {return NULL_STRING;}
	jnullptr getAsJsonNull() {return JSON_NULL_INSTANCE;}
	static jnullptr make() {return JSON_NULL_INSTANCE;}
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
};

class JsonBoolean: public JsonPrimitive {
public:
	JsonType getJsonType() const {return JsonType::JBOOLEAN;};
	bool isBoolean() const {return true;}
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
};

class JsonTrue: public JsonBoolean {
public:
	bool booleanValue() const {return true;}
	void appendToString(std::string& str) const {str+=TRUE_STRING;}
	void print(std::ostream& out) const {out<<TRUE_STRING;}
	const std::string& stringValue() const {return TRUE_STRING;}
	jboolptr getAsJsonBoolean() const {return JSON_TRUE_INSTANCE;}
};

class JsonFalse: public JsonBoolean {
public:
	bool booleanValue() const {return false;}
	void appendToString(std::string& str) const {str+=FALSE_STRING;}
	void print(std::ostream& out) const {out<<FALSE_STRING;}
	const std::string& stringValue() const {return FALSE_STRING;}
	jboolptr getAsJsonBoolean() const {return JSON_FALSE_INSTANCE;}
};

class JsonDataPrimitive: public JsonPrimitive {
protected:
	std::string raw;
public:
	template <typename T> JsonDataPrimitive(T&& v): raw(std::forward<T>(v)) {}
	template <typename T> void set(T&& t) {raw=std::forward<T>(t);}
	const std::string& stringValue() const {return raw;}
	std::string& rawStringValue() {return raw;}
	void appendToString(std::string& str) const {str+=raw;}
};

class JsonString: public JsonDataPrimitive {
public:
	JsonType getJsonType() const {return JsonType::JSTRING;};
	jstrptr getAsJsonString() const {return std::static_pointer_cast<JsonString,JsonElement>(const_cast<JsonString*>(this)->shared_from_this());}
	template <typename T> JsonString(T&& t) : JsonDataPrimitive(std::forward<T>(t)) {}
	template <typename ARG> static jstrptr make(ARG&& arg) {return std::make_shared<JsonString>(std::forward<ARG>(arg));}
	void appendToString(std::string& str) const;
	void print(std::ostream& out) const;
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
};
class JsonNumber: public JsonDataPrimitive {
public:
	JsonType getJsonType() const {return JsonType::JNUMBER;};
	JsonNumber(int64_t t) : JsonDataPrimitive(std::to_string(t)) {}
	JsonNumber(int32_t t) : JsonDataPrimitive(std::to_string(t)) {}
	JsonNumber(double t) : JsonDataPrimitive(std::to_string(t)) {}
	JsonNumber(float t) : JsonDataPrimitive(std::to_string(t)) {}
	template <typename T> JsonNumber(T&& t) : JsonDataPrimitive(std::forward<T>(t)) {}
	int64_t longValue() const {return std::stoll(raw);}
	double doubleValue() const {return std::stod(raw);}
	jnumptr getAsJsonNumber() const {return std::static_pointer_cast<JsonNumber,JsonElement>(const_cast<JsonNumber*>(this)->shared_from_this());}
	void appendToString(std::string& str) const {str+=raw;}
	void print(std::ostream& out) const {out<<raw;}
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
};


class JsonObject: public JsonElement {
	typedef std::list<std::string> KeyList;
	typedef std::tuple<jptr,std::list<std::string>::iterator> DataTuple;
	std::list<std::string> keyList;
	struct NameLess {bool operator()(const char* a, const char* b) const noexcept {return ::strcmp(a,b)<0;}};
	struct NameEq {bool operator()(const char* a, const char* b) const noexcept {return ::strcmp(a,b)==0;;}};
	//struct NameHash {std::size_t operator()(const std::string* k) const noexcept {return std::hash<std::string>()(*k);}};
	//std::unordered_map<const std::string*, DataTuple, NameHash, NameEq> memberMap{};
	typedef std::map<const char*, DataTuple, NameLess>::iterator MemeberIterator;
	std::map<const char*, DataTuple, NameLess> memberMap{};

public:
	JsonType getJsonType() const {return JsonType::JOBJECT;}
	bool isJsonObject() const {return true;}
	jobjptr getAsJsonObject() const {return std::static_pointer_cast<JsonObject,JsonElement>(const_cast<JsonObject*>(this)->shared_from_this());}
	template <typename V> void addNotEmptyChild(const char* cstr, V&& child);
	template <typename P, typename V> void addNotEmptyChild(P&& p, V&& child);
	template <typename P, typename V> void addChild(P&& p, V&& child);

	//void add(const char* p, const jptr& c) {addChild(p, c);}
	template <typename P> void add(P&& p, const jptr& c) {addChild(std::forward<P>(p), c);}
	template <typename P> void add(P&& p, jptr&& c) {addChild(std::forward<P>(p), std::move(c));}

	template <typename P> void add(P&& p, const std::string& v) {addNotEmptyChild(std::forward<std::string>(p), std::make_shared<JsonString>(v));}
	template <typename P> void add(P&& p, std::string&& v) {addNotEmptyChild(std::forward<P>(p), std::make_shared<JsonString>(std::move(v)));}
	template <typename P> void add(P&& p, const char* v) {if (v) addNotEmptyChild(std::forward<P>(p),std::make_shared<JsonString>(v)); else addNotEmptyChild(std::forward<std::string>(p),JSON_NULL_INSTANCE);}
	template <typename P> void add(P&& p, int64_t v) {addNotEmptyChild(std::forward<P>(p), std::make_shared<JsonNumber>(v));}
	template <typename P> void add(P&& p, int32_t v) {addNotEmptyChild(std::forward<P>(p), std::make_shared<JsonNumber>(v));}
	template <typename P> void add(P&& p, double v) {addNotEmptyChild(std::forward<P>(p), std::make_shared<JsonNumber>(v));}
	template <typename P> void add(P&& p, bool v) {addNotEmptyChild(std::forward<P>(p), makeJsonBoolean(v));}
	template <typename P> void addNull(P&& p) {addNotEmptyChild(std::forward<P>(p), JSON_NULL_INSTANCE);}
	template <typename P> jobjptr addJsonObject(P&& p);
	template <typename P> jarrptr addJsonArray(P&& p);

	jptr get(const std::string& p) {return get(p.c_str());}
	jptr get(const char* p) {
		auto it=memberMap.find(p);
		return (it==memberMap.end()) ? jptr() : std::get<0>(it->second);
	}
	jptr remove(const char* p);
	jptr remove(const std::string& p) {return remove(p.c_str());}
	void appendToString(std::string& str) const;
	void print(std::ostream& out) const;
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
	bool simple() const;
	friend class JsonObjectIterator;
	friend class JsonObjectKeyValuePairs;
	friend class JsonKeyIterator;
	friend class JsonObjectKeys;
	friend class JsonObjectSortedIterator;
	friend class JsonSortedKeyIterator;
};
template <typename V>
inline void JsonObject::addNotEmptyChild(const char* cstr, V&& child) {
	auto it=memberMap.find(cstr);
	if (it==memberMap.end()) {
		keyList.emplace_back(cstr);
		auto listPtr=std::prev(keyList.end());
		auto addr=keyList.back().c_str();
		memberMap.emplace(std::piecewise_construct, std::forward_as_tuple(addr), std::forward_as_tuple(std::forward<V>(child), listPtr));
	} else {
		auto& tuple=it->second;
		std::get<0>(tuple)=std::forward<V>(child);
	}
}
template <typename P, typename V>
inline void JsonObject::addNotEmptyChild(P&& p, V&& child) {
	auto it=memberMap.find(p.c_str());
	if (it==memberMap.end()) {
		keyList.emplace_back(std::forward<P>(p));
		auto listPtr=std::prev(keyList.end());
		auto addr=keyList.back().c_str();
		memberMap.emplace(std::piecewise_construct, std::forward_as_tuple(addr), std::forward_as_tuple(std::forward<V>(child), listPtr));
	} else {
		auto& tuple=it->second;
		std::get<0>(tuple)=std::forward<V>(child);
	}
}
template <typename P, typename V>
inline void JsonObject::addChild(P&& p, V&& child) {
	if (child) {
		addNotEmptyChild(std::forward<P>(p),std::forward<V>(child));
	} else {
		addNotEmptyChild(std::forward<P>(p),JSON_NULL_INSTANCE);
	}
}

template <typename P> inline jobjptr JsonObject::addJsonObject(P&& p) {
	auto obj=std::make_shared<JsonObject>();
	addNotEmptyChild(std::forward<std::string>(p), obj);
	return obj;
}

inline jptr JsonObject::remove(const char* cstr) {
	auto it=memberMap.find(cstr);
	if (it==memberMap.end()) return jptr();
	auto& tuple=it->second;
	jptr ret=std::move(std::get<0>(tuple));
	std::list<std::string>::iterator rmItPtr=std::get<1>(tuple);
	keyList.erase(rmItPtr);
	memberMap.erase(it);
	return ret;
}

class JsonObjectIterator {
	JsonObject* ptr;
	JsonObject::KeyList::iterator it;
public:
	explicit JsonObjectIterator() :ptr(nullptr), it() {}
	explicit JsonObjectIterator(const JsonObject* p) :  ptr(const_cast<JsonObject*>(p)), it(ptr?const_cast<JsonObject*>(p)->keyList.begin():JsonObject::KeyList::iterator()) {}
	explicit JsonObjectIterator(const jobjptr& p) : JsonObjectIterator(p.get()) {}
	explicit JsonObjectIterator(const JsonElement* p) : JsonObjectIterator(const_cast<JsonElement*>(p)->getAsJsonObject().get()) {}
	explicit JsonObjectIterator(const jptr& p) : ptr( p ? p->getAsJsonObject().get() : nullptr), it( ptr? ptr->keyList.begin():JsonObject::KeyList::iterator()) {}

	std::pair<const std::string&,jptr&> operator *() {
		std::string& key=*it;
		auto& tuple=ptr->memberMap.find(key.c_str())->second;
		auto& jchild=std::get<0>(tuple);
		return std::pair<const std::string&,jptr&>(key,jchild);
	}
	JsonObjectIterator& operator++() {it=std::next(it);return *this;}
	bool valid() const {return ptr!=nullptr && it!=ptr->keyList.end();}
	operator bool() const {return valid();}
	bool operator !=(const JsonObjectIterator& rhs) const {return valid() ? it!=rhs.it : false;}
	void remove() {
		ptr->memberMap.erase(it->c_str());
		it=ptr->keyList.erase(it);
	}
};


class JsonObjectPairs {
	JsonElement* ptr;
public:
	explicit JsonObjectPairs() :ptr(nullptr) {}
	explicit JsonObjectPairs(const jptr& p) :  ptr(p.get()) {}
	JsonObjectIterator begin() const {return JsonObjectIterator(ptr);}
	JsonObjectIterator end() const {return JsonObjectIterator();}
};

class JsonKeyIterator {
	JsonObject* ptr;
	JsonObject::KeyList::iterator it;
public:
	explicit JsonKeyIterator() :ptr(nullptr), it() {}
	explicit JsonKeyIterator(const jobjptr& p) :  JsonKeyIterator(p.get()) {}
	explicit JsonKeyIterator(const JsonObject* p) :  ptr(const_cast<JsonObject*>(p)), it(p?const_cast<JsonObject*>(p)->keyList.begin():JsonObject::KeyList::iterator()) {}
	explicit JsonKeyIterator(const JsonElement* p) : JsonKeyIterator(const_cast<JsonElement*>(p)->getAsJsonObject().get()) {}
	explicit JsonKeyIterator(const jptr& p) :JsonKeyIterator(p.get()) {}

	const std::string& operator *() {
		return *it;
	}
	void operator++() {
		it=std::next(it);
	}
	bool valid() const {return ptr!=nullptr && it!=ptr->keyList.end();}
	operator bool() const {return valid();}
	bool operator !=(const JsonKeyIterator& rhs) const {return valid() ? it!=rhs.it : false;}

	void remove() {
		ptr->memberMap.erase(it->c_str());
		it=ptr->keyList.erase(it);
	}
};

class JsonObjectKeys {
	JsonElement* ptr;
public:
	explicit JsonObjectKeys() :ptr(nullptr) {}
	explicit JsonObjectKeys(const jptr& p) :  ptr(p.get()) {}
	JsonKeyIterator begin() const {return JsonKeyIterator(ptr);}
	JsonKeyIterator end() const {return JsonKeyIterator();}
};

class JsonObjectSortedIterator {
	JsonObject* ptr;
	JsonObject::MemeberIterator it;
public:
	explicit JsonObjectSortedIterator() :ptr(nullptr), it() {}
	explicit JsonObjectSortedIterator(const JsonObject* p) :  ptr(const_cast<JsonObject*>(p)), it(p?const_cast<JsonObject*>(p)->memberMap.begin():JsonObject::MemeberIterator()) {}
	explicit JsonObjectSortedIterator(const jobjptr& p) : JsonObjectSortedIterator(p.get()) {}
	explicit JsonObjectSortedIterator(const JsonElement* p) : JsonObjectSortedIterator(const_cast<JsonElement*>(p)->getAsJsonObject().get()) {}
	explicit JsonObjectSortedIterator(const jptr& p) : JsonObjectSortedIterator(p.get()) {}

	std::pair<const std::string&,jptr&> operator *() {
		auto& tuple=it->second;
		auto& jchild=std::get<0>(tuple);
		auto& keyIt=std::get<1>(tuple);
		return std::pair<const std::string&,jptr&>(*keyIt,jchild);
	}
	JsonObjectSortedIterator& operator++() {it=std::next(it);return *this;}
	bool valid() const {return ptr!=nullptr && it!=ptr->memberMap.end();}
	operator bool() const {return valid();}
	bool operator !=(const JsonObjectSortedIterator& rhs) const {return valid() ? it!=rhs.it : false;}
	void remove() {
		auto& tuple=it->second;
		auto& keyIt=std::get<1>(tuple);
		ptr->keyList.erase(keyIt);
		it=ptr->memberMap.erase(it);
	}
};

class JsonSortedKeyIterator {
	JsonObject* ptr;
	JsonObject::MemeberIterator it;
public:
	explicit JsonSortedKeyIterator() :ptr(nullptr), it() {}
	explicit JsonSortedKeyIterator(const JsonObject* p) :  ptr(const_cast<JsonObject*>(p)), it(p?const_cast<JsonObject*>(p)->memberMap.begin():JsonObject::MemeberIterator()) {}
	explicit JsonSortedKeyIterator(const jobjptr& p) : JsonSortedKeyIterator(p.get()) {}
	explicit JsonSortedKeyIterator(const JsonElement* p) : JsonSortedKeyIterator( const_cast<JsonElement*>(p)->getAsJsonObject().get()) {}
	explicit JsonSortedKeyIterator(const jptr& p) : JsonSortedKeyIterator(p.get()) {}

	const std::string& operator *() {
		auto& tuple=it->second;
		auto& keyIt=std::get<1>(tuple);
		return *keyIt;
	}
	JsonSortedKeyIterator& operator++() {
		it=std::next(it);
		return *this;
	}
	bool valid() const {return ptr!=nullptr && it!=ptr->memberMap.end();}
	operator bool() const {return valid();}
	bool operator !=(const JsonSortedKeyIterator& rhs) const {return valid() ? it!=rhs.it : false;}
	void remove() {
		auto& tuple=it->second;
		auto& keyIt=std::get<1>(tuple);
		ptr->keyList.erase(keyIt);
		it=ptr->memberMap.erase(it);
	}
};

class JsonObjectSortedPairs {
	JsonElement* ptr;
public:
	explicit JsonObjectSortedPairs() :ptr(nullptr) {}
	explicit JsonObjectSortedPairs(const jptr& p) :  ptr(p.get()) {}
	JsonObjectSortedIterator begin() const {return JsonObjectSortedIterator(ptr);}
	JsonObjectSortedIterator end() const {return JsonObjectSortedIterator();}
};

class JsonObjectSortedKeys {
	JsonElement* ptr;
public:
	explicit JsonObjectSortedKeys() :ptr(nullptr) {}
	explicit JsonObjectSortedKeys(const jptr& p) :  ptr(p.get()) {}
	JsonSortedKeyIterator begin() const {return JsonSortedKeyIterator(ptr);}
	JsonSortedKeyIterator end() const {return JsonSortedKeyIterator();}
};

class JsonArray: public JsonElement {
	//std::list<jptr> elements;
	//typedef std::vector<jptr> Elements;
	typedef std::list<jptr> Elements;
	Elements elements;
public:
	JsonType getJsonType() const {return JsonType::JARRAY;}
	bool isJsonArray() const {return true;}
	jarrptr getAsJsonArray() const {return std::static_pointer_cast<JsonArray,JsonElement>(const_cast<JsonArray*>(this)->shared_from_this());}


	template <typename C> std::shared_ptr<C> add() {auto ret=std::make_shared<C>(); elements.emplace_back(ret); return ret;}
	template <typename C> jptr add(std::shared_ptr<C>&& e) {
		if (e) return elements.emplace_back(std::forward<std::shared_ptr<C>>(e));
		else return elements.emplace_back(JSON_NULL_INSTANCE);
	}
	template <typename C> jptr addNotEmpty(std::shared_ptr<C>&& e) {
		return elements.emplace_back(std::forward<std::shared_ptr<C>>(e));
	}
	template <typename C> jptr addNotEmpty(const std::shared_ptr<C>& e) {
		return elements.emplace_back(e);
	}


	template <typename C> jptr add(const std::shared_ptr<C>& e) {
		if (e) return elements.emplace_back(e);
		else return elements.emplace_back(JSON_NULL_INSTANCE);
	}

	void add(const std::string& v);
	void add(std::string&& v);
	void add(const char* v);
	void add(int64_t v);
	void add(int32_t v);
	void add(double v);
	void add(bool v);
	void addNull();
	jobjptr addJsonObject() {return add<JsonObject>();}
	jarrptr addJsonArray() {return add<JsonArray>();}
	void appendToString(std::string& str) const;
	void print(std::ostream& out) const;
	void prettyPrint(std::ostream& out, int off=0, int policy=0) const;
	bool empty() const {return elements.begin()==elements.end();}
	bool single() const {return elements.begin()!=elements.end() && std::next(elements.begin())==elements.end();}
	bool simple() const;
	friend class JsonArrayIterator;
};

template <typename P> inline jarrptr JsonObject::addJsonArray(P&& p) {
	auto arr=std::make_shared<JsonArray>();
	addNotEmptyChild(std::forward<decltype(p)>(p), arr);
	return arr;

}


inline jboolptr makeJsonBoolean(bool val) {return val?JSON_TRUE_INSTANCE:JSON_FALSE_INSTANCE;}
inline jnullptr makeJsonNull() {return JSON_NULL_INSTANCE;}
inline jobjptr makeJsonObject() {return std::make_shared<JsonObject>();}
inline jarrptr makeJsonArray() {return std::make_shared<JsonArray>();}
template <typename ARG> inline jnumptr makeJsonNumber(ARG&& arg) {return std::make_shared<JsonNumber>(std::forward<ARG>(arg));}
template <typename ARG> inline jstrptr makeJsonString(ARG&& arg) {return std::make_shared<JsonString>(std::forward<ARG>(arg));}

inline void JsonArray::add(const std::string& v) {elements.emplace_back(makeJsonString(v));}
inline void JsonArray::add(std::string&& v) {elements.emplace_back(makeJsonString(std::forward<std::string>(v)));}
inline void JsonArray::add(const char* v) {
	if (v) elements.emplace_back(makeJsonString(v));
	else elements.emplace_back(JSON_NULL_INSTANCE);
}
inline void JsonArray::add(int64_t v) {elements.emplace_back(makeJsonNumber(v));}
inline void JsonArray::add(int32_t v) {elements.emplace_back(makeJsonNumber(v));}
inline void JsonArray::add(double v) {elements.emplace_back(makeJsonNumber(v));}
inline void JsonArray::add(bool v) {elements.emplace_back(makeJsonBoolean(v));}
inline void JsonArray::addNull() {elements.emplace_back(JSON_NULL_INSTANCE);}

class JsonArrayIterator {
	JsonArray* ptr;
	JsonArray::Elements::iterator it;
public:
	explicit JsonArrayIterator() :ptr(nullptr), it() {}
	explicit JsonArrayIterator(const JsonArray* p) :  ptr(const_cast<JsonArray*>(p)), it(ptr?ptr->elements.begin():JsonArray::Elements::iterator()) {}
	explicit JsonArrayIterator(const jarrptr& p) : JsonArrayIterator(p.get()) {}
	explicit JsonArrayIterator(const JsonElement* p) : JsonArrayIterator(const_cast<JsonElement*>(p)->getAsJsonArray().get()) {}
	explicit JsonArrayIterator(const jptr& p) : JsonArrayIterator(p.get()) {}

	jptr& operator *() const {return *it;}
	JsonArrayIterator& operator++() {it=std::next(it);return *this;}
	bool valid() const {return ptr!=nullptr && it!=ptr->elements.end();}
	operator bool() const {return valid();}
	bool operator !=(const JsonArrayIterator& rhs) const {return valid() ? it!=rhs.it : false;}
	void remove() {
		it=ptr->elements.erase(it);
	}
};

class JsonArrayElements {
	JsonElement* ptr;
public:
	explicit JsonArrayElements() :ptr(nullptr) {}
	explicit JsonArrayElements(const jptr& p) :  ptr(p.get()) {}
	JsonArrayIterator begin() const {return JsonArrayIterator(ptr);}
	JsonArrayIterator end() const {return JsonArrayIterator();}
};

inline void collectPath(std::string&) {}
template <typename... REST> inline void collectPath(std::string str, const char* first, REST... rest) {
	str+=first;
	if (sizeof...(rest) > 0) str+='.';
	collectPath(str,rest...);
}
inline jptr getJsonElement(const jptr& j) {return j;}
template <typename... REST> inline jptr getJsonElement(const jptr& p, const std::string& first, const REST&... rest) {
	if (!p) return jptr();
	auto obj=p->getAsJsonObject();
	return obj ? getJsonElement(obj->get(first.c_str()), rest...) : jptr();
}
template <typename... REST> inline jptr getJsonElement(const jptr& p, const char* first, const REST&... rest) {
	if (!p) return jptr();
	auto obj=p->getAsJsonObject();
	return obj ? getJsonElement(obj->get(first),rest...) : jptr();
}

template <typename... REST> inline jstrptr getJsonString(const jptr& p, const REST&... rest) {
	auto e=getJsonElement(p, rest...);
	return e ? e->getAsJsonString() : jstrptr();
}
template <typename... REST> inline std::string* getStringPtr(const jptr& p, const REST&... rest) {
	jstrptr s=getJsonString(p, rest...);
	return s ? &s->rawStringValue() : nullptr;
}
template <typename... REST> inline jnumptr getJsonNumber(const jptr& p, const REST&... rest) {
	auto e=getJsonElement(p, rest...);
	return e ? e->getAsJsonNumber() : jnumptr();
}
template <typename... REST> inline std::optional<int64_t> getLong(const jptr& p, const REST&... rest) {
	auto n=getJsonNumber(p, rest...);
	return n ? n->longValue() : std::optional<int64_t>();
}
template <typename... REST> inline std::optional<double> getDouble(const jptr& p, const REST&... rest) {
	auto n=getJsonNumber(p, rest...);
	return n ? n->doubleValue() : std::optional<double>();
}
template <typename... REST> inline std::optional<bool> getBoolean(const jptr& p, const REST&... rest) {
	jboolptr b=getJsonBoolean(p, rest...);
	return b ? b->booleanValue() : std::optional<bool>();
}

template <typename... REST> inline jboolptr getJsonBoolean(const jptr& p, const REST&... rest) {
	auto e=getJsonElement(p, rest...);
	return e ? e->getAsJsonBoolean() : jboolptr();
}
template <typename... REST> inline jnullptr getJsonNull(const jptr& p, const REST&... rest) {
	auto e=getJsonElement(p, rest...);
	return e ? e->getAsJsonNull() : jnullptr();
}

void print(std::ostream& out,const JsonElement* el);

struct Pretty {
	JsonElement* ptr;
	Pretty(const jptr& p): ptr(p.get()) {}
	void print(std::ostream& out);
};


jptr parseArrayOrObject(const char* start, const char* end, size_t* consumedBytesPtr=nullptr);
inline jptr parseArrayOrObject(const char* start, size_t* consumedBytesPtr=nullptr) {
	if (start) return parseArrayOrObject(start, start+::strlen(start), consumedBytesPtr);
	else return jptr();
}
inline jptr parseArrayOrObject(const std::string& str, size_t* consumedBytesPtr=nullptr) {
	return parseArrayOrObject(str.c_str(), str.c_str()+str.size(), consumedBytesPtr);
}

jptr parseFromFile(const char* fileName, size_t pos=0, size_t* consumed=nullptr);

}

std::ostream& operator<<(std::ostream& out,const json::Pretty& p);
inline std::ostream& operator<<(std::ostream& out,const json::jptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jarrptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jobjptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jstrptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jnumptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jboolptr& p) {json::print(out,p.get());return out;}
inline std::ostream& operator<<(std::ostream& out,const json::jnullptr& p) {json::print(out,p.get());return out;}



#endif /* JSONUTILS_H_ */
