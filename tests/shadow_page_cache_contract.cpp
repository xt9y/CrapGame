#include "Renderer/Gpu/ShadowPageCachePolicy.hpp"

#include <cstdlib>
#include <iostream>
#include <vector>

static void require(bool value,const char *message)
{
    if(!value){std::cerr<<message<<'\n';std::exit(1);}
}

int main()
{
    using namespace Renderer::Gpu;

    ShadowPageKey a={3u,6u,0u,4,7};
    ShadowPageKey b=a;
    ShadowPageKey c=a;c.y=8;
    require(sameShadowPageKey(a,b),"same page key must match");
    require(!sameShadowPageKey(a,c),"different page coordinate must not match");

    std::vector<ShadowPageState> pages(3);
    pages[0].allocated=true;pages[0].last_requested=10u;
    pages[1].allocated=true;pages[1].last_requested=40u;
    pages[2].allocated=true;pages[2].last_requested=100u;
    require(chooseShadowPageEviction(pages,100u)==0,"oldest page evicts first");
    pages[0].last_requested=100u;
    require(chooseShadowPageEviction(pages,100u)==1,"current-frame request is protected");
    pages[1].last_requested=100u;
    require(chooseShadowPageEviction(pages,100u)==-1,"all current-frame pages are protected");

    std::cout<<"shadow_page_cache_contract=PASS\n";
}
