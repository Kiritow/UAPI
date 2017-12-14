#include "util.h"
#include <iostream>
using namespace std;

int main()
{
    cout<<"Program Start!"<<endl;

    HTTPRequest req;
    req.host="localhost";
    HTTPResponse res;
    int ret=req.send(res);
    cout<<"ReturnCode: "<<ret<<endl;

    cout<<"Response Protocol: "<<res.protocol<<endl;
    cout<<"Response Status: "<<res.status<<endl;
    cout<<"Response Content Type: "<<res.content_type<<endl;
    cout<<"Response Content Length: "<<res.content_length<<endl;

    cout<<"Response Document:"<<endl;
    cout<<res.content<<endl;

    cout<<"Program Finished."<<endl;
    return 0;
}
