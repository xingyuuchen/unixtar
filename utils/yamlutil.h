#pragma once
#include <cstdlib>
#include <string>
#include <vector>


namespace yaml {


class Node;
class AbsValue;

class ValueLeaf;
class ValueObj;
class ValueArray;


/**
 *
A yaml file may goes like this:
-------------------------------------------
port: 80
net_thread_cnt: 4
xxx:
    yyy: 123
    zzz: 456
webservers:
    -
        ip: 127.0.0.1
        port: 5002
        weight: 1
    -
        ip: 127.0.0.1
        port: 5003
        weight: 2
-------------------------------------------
 *
 */

class Node {
  public:
    Node();
    virtual ~Node();
    std::string     key;
    AbsValue      * value;
};


struct YamlDescriptor : public Node {
    YamlDescriptor();
    
    ~YamlDescriptor() override;
    
    FILE *Fp() const;
    
    void Fp(FILE *_fp);
    
    ValueLeaf *GetLeaf(char const *_key);
    
    ValueArray *GetArray(char const *_key);
    
    ValueObj *GetYmlObj(char const *_key);
    
  private:
    FILE    * fp;
};


struct AbsValue {
  public:
    enum TType {
        kUnknown = 0,
        kLeaf,
        kObject,
        kArray,
    };
    virtual ~AbsValue();
    
    virtual TType Type() = 0;
    
    virtual void Insert(Node *) = 0;
    
};


struct ValueLeaf : public AbsValue {
    ~ValueLeaf() override;
    
    TType Type() override;
    
    void Insert(Node *) override;
    
    int To(int &_res) const;
    
    int To(uint16_t &_res) const;
    
    int To(double &_res) const;
    
    int To(std::string &_res) const;
    
    std::string data;
};


struct ValueObj : public AbsValue {
    ~ValueObj() override;
    
    TType Type() override;
    
    void Insert(Node *) override;
    
    ValueLeaf *GetLeaf(char const *_key);
    
    ValueObj *GetYmlObj(char const *_key);
    
    ValueArray *GetArr(char const *_key);
    
    std::vector<Node *> nodes;
    
  private:
    AbsValue *__Get(char const *_key);
    
    friend struct YamlDescriptor;
};


struct ValueArray : public AbsValue {
    ValueArray();
    
    ~ValueArray() override;
    
    TType Type() override;
    
    void Insert(Node *) override;
    
    void InsertValueLeaf(ValueLeaf *);
    
    void MarkArrStart();
    
    std::vector<AbsValue *> arr;
    
    std::vector<ValueObj *> &AsYmlObjects();
    
    std::vector<ValueLeaf *> &AsYmlLeaves();
    
  private:
    int curr;
    int len_of_obj_in_arr;
};


YamlDescriptor *Load(const char *_path);

bool IsCached(YamlDescriptor *_desc);

int Close(YamlDescriptor *_desc);


}
