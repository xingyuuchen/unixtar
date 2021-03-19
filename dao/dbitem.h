#pragma once
#include <string>
#include <vector>
#include <memory>


class DBItem {
  public:
    /**
     * Called by mysql++ framework.
     */
    virtual std::string& field_list() const final;
    
    /**
     * Called by mysql++ framework.
     */
    virtual std::string& value_list() const final;
    
    /**
     *
     * Called by mysql++ framework.
     *
     * @return: table name, must stored in static memory.
     */
    virtual const char *table() const = 0;
    
    /**
     *
     * @param _filed_list: where to populate all field-names in.
     */
    virtual void PopulateFieldList(std::vector<std::string> &_filed_list) const = 0;
    
    /**
     *
     * @param _value_list: where to populate all values in.
     */
    virtual void PopulateValueList(std::vector<std::string> &_value_list) const = 0;
    
    /**
     * Callback when the preparation of custom data is done.
     */
    void OnDataPrepared();
    
  private:
    void __Format(std::vector<std::string> &_in, std::string&_out);
    
  private:
    std::string field_list_str_;
    std::string value_list_str_;
};
