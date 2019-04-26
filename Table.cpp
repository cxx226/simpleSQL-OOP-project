#include "Table.h"

Table::Table(string name, vector<Attribute> a, string primary)
        :name(name), attrs(a), primary(primary) 
{}

Table::Table(Table && t) {}

Table::~Table() {}

bool Table::insert(vector<string> attrNames, vector<ValueBase *> vals) {
    Record t(attrs.size());
    bool succ = true;
    for (int i = 0; i < attrNames.size(); i++) {
        for (int j = 0; j < attrs.size(); j++) {
            if (attrNames[i] == attrs[j].name) {
                // 检查是否指定了重复的属性
                for (int k = 0; k < attrNames.size(); k ++) 
                    if (attrNames[i]== attrNames[k])
                        succ = false;
                switch (attrs[j].type)
                {
                    case ATTR_INT:
                    {
                        auto intv = dynamic_cast<Value<int>* >(vals[i]);
                        if (intv) {
                            t[j] = new Value<int>(int(*intv));
                        } else succ = false;
                        break;
                    }
                    case ATTR_DOUBLE:
                    {
                        auto doublev = dynamic_cast<Value<double>* >(vals[i]);
                        if (doublev) {
                            t[j] = new Value<double>(double(*doublev));
                        } else succ = false;
                        break;
                    }
                    case ATTR_CHAR:
                    {
                        auto charv = dynamic_cast<Value<string>* >(vals[i]);
                        if (charv) {
                            t[j] = new Value<string>(string(*charv));
                        } else succ = false;
                        break;
                    }
                    default:
                        break;
                }
                break;
            }
        }
        if (!succ) break;
    }
    for (int i = 0; i < attrs.size(); i++) {
        // notNull check(primary key is forced to be notNull)
        if (t[i] == nullptr && (attrs[i].notNull || primary == attrs[i].name)) {
            succ = false;
            break;
        }
        // primary key
        if (primary == attrs[i].name) {
            for (auto& j: data) {
                if (*j[i] == *t[i]) {
                    succ = false;
                    break;
                }
            }
        }
        if (!succ) break;
    }
    if (succ) data.push_back(std::move(t));
    else {
        for (int i = 0; i < attrs.size(); i++) 
            delete t[i];
    }
    return succ;
}

bool Table::del(WhereClause c) {
    // select elements in a reverse order
    for (int i = data.size()-1; i>= 0; i-- ) {
        if (c.test(data[i], attrs)) {
            data.erase(data.begin() + i);
        }
    }
    return true;
}
bool Table::checkType(AttributeType att, ValueBase * v) {
    if (att == ATTR_CHAR && !dynamic_cast<Value<string>*> (v) )
        return false;
    if (att == ATTR_DOUBLE && !dynamic_cast<Value<double>*> (v) )
        return false;
    if (att == ATTR_INT && !dynamic_cast<Value<int>*> (v) )
        return false;
    return true;
}
bool Table::update(vector<string> attrNames, vector<ValueBase *> vals, WhereClause c) {
    // 类型匹配，非空检查
    for (int j = 0; j < attrNames.size(); j++) {
        for (int k = 0; k < attrs.size(); k++) {
            if (attrNames[j] == attrs[k].name) {
                if (!checkType(attrs[k].type, vals[j]))
                    return false;
                if (attrs[k].notNull && vals[j] == nullptr) 
                    return false;
                break;
            }
        }
    }
    // 主键查重
    vector<int> updateList; // 待修改的行号记录在这里
    // primaryCount: 主键的新值在更新完成后出现了多少次；
    // primaryIndex: 主键在attrs中的下标索引是多少
    int primaryCount = 0, primaryIndex = 0; 
    // primaryValue: 主键的新值是多少（如果主键未被修改，则为nullptr）
    ValueBase * primaryValue = nullptr;
    for (int i = 0; i < attrs.size(); i++) {
        if (attrs[i].name == primary) {
            primaryIndex = i;
            break;
        }
    }
    for (int i = 0 ; i < vals.size(); i++) {
        if (attrNames[i] == primary) 
            primaryValue = vals[i];
    }
    // 合法，则可以进行修改
    for (int i = 0; i < data.size(); i ++) {
        if (!c.test(data[i], attrs)) {
            if (primaryValue) 
                primaryCount += *data[i][primaryIndex] == *primaryValue;
        } else {
            updateList.push_back(i);
            primaryCount ++; // 对于赋值型更新，每修改一个，重复次数多一次
        }
        if (primaryValue && primaryCount > 1) break;
    }
    if (primaryValue && primaryCount > 1) return false;
    for (auto i: updateList) {
        for (int j = 0; j < attrNames.size(); j++) {
            for (int k = 0; k < attrs.size(); k++) {
                if (attrNames[j] == attrs[k].name) {
                    delete data[i][k];
                    data[i][k] = vals[j]->copy();
                    break;
                }
            }
        }
    }
    return true;
}

PrintableTable * Table::select(vector<string> attrFilter, WhereClause c) {
    vector<Attribute> newTableAttrs;
    for (auto i: attrs) {
        bool flag = false;
        for (auto j: attrFilter) {
            if (j == "*" || j == i.name) {
                flag = true;
                break;
            }
        }
        if (flag) 
            newTableAttrs.push_back(i);
    }
    // 这里主键名称为空，不符合约定，因此
    PrintableTable * table = new PrintableTable(newTableAttrs);
    int n = attrs.size(), m = newTableAttrs.size();
    for (auto r: data) {
        if (!c.test(r, attrs)) continue;
        auto nr = new ValueBase *[m];
        for (int i = 0, j = 0; i < n; i++) {
            if (j == m || attrs[i].name == newTableAttrs[j].name) {
                nr[j] = r[i]->copy();
                j++;
            }
        }
        table->insert(nr);
    }
    return table;
}