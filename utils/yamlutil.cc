#include "yamlutil.h"
#include "log.h"
#include "strutil.h"
#include <cstdio>
#include <string>
#include <fstream>
#include <set>
#include <stack>
#include <cassert>


namespace yaml {

static std::set<YamlDescriptor *> sg_yaml_cache;

static int ParseYaml(const char *_path, YamlDescriptor *_desc);

static Node *GetParentNodeByIndentDiff(std::stack<Node *> &_stack,
                                       int _indent_diff);

// debug only
static void UnitTest();
static void PrintYamlTree(YamlDescriptor *_desc);
static void PrintNode(Node *_node);


/**
 *
 * Tab should be 2 spaces.
 *
 * @param _path
 * @param _desc
 * @return
 */
static int ParseYaml(const char *_path, YamlDescriptor *_desc) {
    std::ifstream fin(_path);
    if (!fin) {
        LogE("!fin: %s", _path)
        assert(false);
    }
    
    std::stack<Node *> stack;
    Node *parent = _desc;
    parent->key = std::string(_path);
    parent->value = new ValueObj();
    
    int last_indents = 0;
    std::string line;
    
    while (getline(fin, line)) {
        str::delafter(line, '#');
        if (line.empty()) { continue; }
        std::vector<std::string> separate;
        str::split(line, ":", separate);
    
        int indents = (int) line.find_first_not_of(' ');
        int indent_diff = indents - last_indents;
    
        if (separate.size() == 2) {
            if (indent_diff < 0) {
                parent = GetParentNodeByIndentDiff(stack, indent_diff);
            }
            auto node = new Node();
            node->key = str::trim(separate[0]);
            auto value = new ValueLeaf();
            value->data = str::trim(separate[1]);
            node->value = value;
            parent->value = parent->value ? parent->value : new ValueObj();
            parent->value->Insert(node);
            
        } else if (separate.size() == 1) {      // "xxx:" or "yyy"
            if (line.find(':') != std::string::npos) {  // "xxx:"
                auto node = new Node();
                node->key = std::string(str::trim(separate[0]));
                if (indent_diff < 0) {
                    parent = GetParentNodeByIndentDiff(stack, indent_diff);
                }
                parent->value->Insert(node);
                stack.push(parent);
                parent = node;
                
                if (indent_diff > 0 && indent_diff != 2) {
                    LogE("%s: incorrect yaml indents, line: %s", _path, line.c_str())
                    assert(false);
                }
                
            } else {   // "yyy"
                assert(parent->value->Type() == AbsValue::kArray);
                auto *value_arr = (ValueArray *) parent->value;
                auto *leaf = new ValueLeaf();
                leaf->data = std::string(str::trim(separate[0]));
                value_arr->InsertValueLeaf(leaf);
            }
    
        } else if (str::trim(line) == "-") {
            parent->value = parent->value ? parent->value : new ValueArray();
            assert(parent->value->Type() == AbsValue::kArray);
            ((ValueArray *) parent->value)->MarkArrStart();
            
        } else {
            LogE("%s: incorrect yaml formula, line: %s", _path, line.c_str())
            assert(false);
        }
        last_indents = indents;
    }
    return 0;
}


static Node *GetParentNodeByIndentDiff(std::stack<Node *> &_stack,
                                       int _indent_diff) {
    LogD("stack size: %ld, indent_diff: %d", _stack.size(), _indent_diff)
    assert(_indent_diff < 0 && _indent_diff % 2 == 0);
    Node *parent = nullptr;
    while (_indent_diff < 0) {
        assert(!_stack.empty());
        parent = _stack.top();
        _stack.pop();
        _indent_diff += 2;
    }
    if (!parent) {
        LogE("parent nullptr, stack size: %ld", _stack.size())
    }
    return parent;
}

YamlDescriptor *Load(const char *_path) {
    for (auto & cache : sg_yaml_cache) {
        if (cache->key == _path) {
            LogD("hit yaml cache: %s", _path)
            return cache;
        }
    }
    FILE *fp = fopen(_path, "r");
    if (fp) {
        auto *desc = new YamlDescriptor();
        desc->Fp(fp);
        ParseYaml(_path, desc);
//        PrintYamlTree(desc);
        if (sg_yaml_cache.size() < 10) {
            sg_yaml_cache.insert(desc);
        }
        return desc;
    }
    LogE("fp == NULL, path: %s", _path)
    return nullptr;
}

int ValueLeaf::To(bool &_res) const {
    if (data == "true") {
        _res = true;
        return 0;
    }
    _res = false;
    return data == "false" ? 0 : -1;
}

int ValueLeaf::To(int &_res) const {
    _res = (int) strtol(data.c_str(), nullptr, 10);
    return 0;
}

int ValueLeaf::To(uint16_t &_res) const {
    _res = (uint16_t) strtol(data.c_str(), nullptr, 10);
    return 0;
}

int ValueLeaf::To(double &_res) const {
    _res = strtod(data.c_str(), nullptr);
    return 0;
}

int ValueLeaf::To(std::string &_res) const {
    _res = std::string(data);
    return 0;
}


int Close(YamlDescriptor *_desc) {
    sg_yaml_cache.erase(_desc);
    int ret = fclose(_desc->Fp());
    delete _desc, _desc = nullptr;
    return ret;
}


bool IsCached(YamlDescriptor *_desc) {
    return sg_yaml_cache.find(_desc) != sg_yaml_cache.end();
}


Node::Node() : value(nullptr) { }
Node::~Node() {
    delete value, value = nullptr;
}

YamlDescriptor::YamlDescriptor() : Node(), fp(nullptr) { }

ValueLeaf *YamlDescriptor::GetLeaf(char const *_key) {
    assert(value->Type() == AbsValue::kObject);
    AbsValue *val = ((ValueObj *) value)->__Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kLeaf) {
        LogE("key(%s) is not a leaf", _key)
        assert(false);
    }
    return (ValueLeaf *) val;
}

ValueArray *YamlDescriptor::GetArray(char const *_key) {
    assert(value->Type() == AbsValue::kObject);
    AbsValue *val = ((ValueObj *) value)->__Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kArray) {
        LogE("key(%s) is not an Array", _key)
        assert(false);
    }
    return (ValueArray *) val;
}

ValueObj *YamlDescriptor::GetYmlObj(char const *_key) {
    assert(value->Type() == AbsValue::kObject);
    AbsValue *val = ((ValueObj *) value)->__Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kObject) {
        LogE("key(%s) is not an Object", _key)
        assert(false);
    }
    return (ValueObj *) val;
}

FILE *YamlDescriptor::Fp() const { return fp; }

void YamlDescriptor::Fp(FILE *_fp) { fp = _fp; }

YamlDescriptor::~YamlDescriptor() = default;

ValueArray::ValueArray() : curr(-1), len_of_obj_in_arr(-1) {}

AbsValue::TType ValueLeaf::Type() { return kLeaf; }

AbsValue::TType ValueArray::Type() { return kArray; }

AbsValue::TType ValueObj::Type() { return kObject; }

void ValueLeaf::Insert(Node *_node) {
    LogPrintStacktrace(4)
    LogE("do not call ValueLeaf::Insert()")
    assert(false);
}



void ValueArray::Insert(Node *_node) {
    ValueObj *obj;
    if (curr == 0) {
        obj = new ValueObj();
        obj->nodes.push_back(_node);
        arr.push_back(obj);
    } else {
        obj = (ValueObj *) arr.back();
        obj->nodes.push_back(_node);
    }
    ++curr;
}

void ValueArray::InsertValueLeaf(ValueLeaf *_leaf) {
    assert(curr == 0);
    arr.push_back(_leaf);
}

void ValueArray::MarkArrStart() {
    if (len_of_obj_in_arr == -1 && curr > 0) {
        len_of_obj_in_arr = curr;
        
    } else if (len_of_obj_in_arr != curr) {
        LogE("objects in yaml array must be of the same length")
        assert(false);
    }
    curr = 0;
}

std::vector<ValueObj *> &ValueArray::AsYmlObjects() {
    assert(len_of_obj_in_arr > 1);
    return (std::vector<ValueObj *> &) arr;
}

std::vector<ValueLeaf *> &ValueArray::AsYmlLeaves() {
    assert(len_of_obj_in_arr == 1);
    return (std::vector<ValueLeaf *> &) arr;
}

void ValueObj::Insert(Node *_node) {
    nodes.push_back(_node);
}

ValueLeaf *ValueObj::GetLeaf(char const *_key) {
    AbsValue *val = __Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kLeaf) {
        LogE("key(%s) is not a leaf", _key)
        assert(false);
    }
    return (ValueLeaf *) val;
}

ValueObj *ValueObj::GetYmlObj(char const *_key) {
    AbsValue *val = __Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kObject) {
        LogE("key(%s) is not an Object", _key)
        assert(false);
    }
    return (ValueObj *) val;
}

ValueArray *ValueObj::GetArr(char const *_key) {
    AbsValue *val = __Get(_key);
    if (!val) {
        return nullptr;
    }
    if (val->Type() != AbsValue::kArray) {
        LogE("key(%s) is not an Object", _key)
        assert(false);
    }
    return (ValueArray *) val;
}

AbsValue *ValueObj::__Get(char const *_key) {
    for (auto &node : nodes) {
        if (node->key == _key) {
            return node->value;
        }
    }
    LogE("no such key: %s", _key)
    return nullptr;
}


AbsValue::~AbsValue() = default;

ValueLeaf::~ValueLeaf() {
//    LogD("delete leaf: %s", data.c_str())
}

ValueObj::~ValueObj() {
    for (auto & node : nodes) {
        delete node, node = nullptr;
    }
}

ValueArray::~ValueArray() {
    for (auto & abs_value : arr) {
        delete abs_value, abs_value = nullptr;
    }
}


static void PrintYamlTree(YamlDescriptor *_desc) {
    PrintNode(_desc);
}

static void PrintNode(Node *_node) {
    LogI("key: %s", _node->key.c_str())
    if (_node->value->Type() == AbsValue::kLeaf) {
        LogI("value: %s", ((ValueLeaf *) _node->value)->data.c_str())
    } else if (_node->value->Type() == AbsValue::kObject) {
        for (auto &node : ((ValueObj *) _node->value)->nodes) {
            PrintNode(node);
        }
    } else if (_node->value->Type() == AbsValue::kArray) {
        for (auto &ele : ((ValueArray *) _node->value)->arr) {
            if (ele->Type() == AbsValue::kLeaf) {
                LogI("value: %s", ((ValueLeaf *) ele)->data.c_str())
            } else {
                auto obj = (ValueObj *) ele;
                for (auto &node : obj->nodes) {
                    PrintNode(node);
                }
            }
        }
    }
}


static void UnitTest() {
    yaml::YamlDescriptor *desc = yaml::Load(
            "/etc/unixtar/proxyserverconf.yml");
    if (!desc) {
        return;
    }
    for (auto & webserver : desc->GetArray("webservers")->AsYmlObjects()) {
        std::string ip;
        uint16_t port;
        uint16_t weight;
        webserver->GetLeaf("ip")->To(ip);
        webserver->GetLeaf("port")->To(port);
        webserver->GetLeaf("weight")->To(weight);
        LogI("%s %d %d", ip.c_str(), port, weight)
    }
    desc->GetYmlObj("aaa")->GetYmlObj("oops");
    std::string ddd;
    desc->GetYmlObj("aaa")
            ->GetYmlObj("ccc")
            ->GetLeaf("ddd")
            ->To(ddd);
    LogI("ddd: %s", ddd.c_str())
    yaml::Close(desc);
}

}
