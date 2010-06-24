#pragma once
#include <vector>
#include <string>
#include <iostream>
#include "CustomOStream.h"

class BaseData;


class BaseObject
{
public:
    BaseObject(std::string name);
    ~BaseObject();

    std::string getClassName() const;
    void processStream(std::ostream& out);
    void addData(BaseData* data);
    const std::vector<BaseData*>& getData() const;
    const std::string& getWarnings() const;
    const std::string& getErrors() const;


    // Serializer les data pour echanger entre threads.

    mutable CustomOStream<BaseObject> sendl;
    mutable std::ostringstream sout;
    mutable std::ostringstream serr;

protected:
    std::vector<BaseData*> vData;
    std::string warnings;
    std::string outputs;
    std::string className;
};
