#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>


class DBItem {
  public:
    /**
     * Called by mysql++ framework, cannot override.
     */
    virtual std::string& field_list() const final;
    
    /**
     * Called by mysql++ framework, cannot override.
     */
    virtual std::string& value_list() const final;
    
    /**
     * Called by mysql++ framework, cannot override.
     */
    virtual std::string &equal_list() const final;
    
    /**
     * Called by mysql++ framework, cannot override.
     */
    template<class T1, class T2>
    std::string &equal_list(T1 &&_t1, T2 &&_t2) const {
        return const_cast<std::string &>(equal_list_str_);
    }
    
    /**
     *
     * Called by mysql++ framework.
     *
     * @return: table name, must stored in static memory.
     */
    virtual const char *table() const = 0;
    
    /**
     *
     * @param _field_list: where to populate all field-names in.
     */
    virtual void PopulateFieldList(std::vector<std::string> &_field_list) const = 0;
    
    /**
     *
     * @param _value_list: where to populate all values in.
     */
    virtual void PopulateValueList(std::vector<std::string> &_value_list) const = 0;
    
    /**
     *
     * @param _equal_list: where to populate all key-values in.
     */
    virtual void PopulateEqualList(std::map<std::string, std::string> &_equal_list) const = 0;
    
    /**
     * Callback when the preparation of custom data is done.
     */
    virtual void OnDataPrepared() final;
    
  private:
    void __Format(std::vector<std::string> &_in, std::string&_out);
    
  private:
    std::string field_list_str_;
    std::string value_list_str_;
    std::string equal_list_str_;
};
