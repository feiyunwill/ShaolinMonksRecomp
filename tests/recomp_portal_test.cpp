#include "portal_effect.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

static void Check(bool value,const char* message) {
    if(!value){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}
}
using Point=std::array<double,3>;
static Point Vertex(std::span<const uint8_t> bytes,unsigned i) {
    std::array<float,3> p;std::memcpy(p.data(),bytes.data()+i*20,12);return {p[0],p[1],p[2]};
}
static double Distance(Point a,Point b) {
    double d=0;for(unsigned j=0;j<3;++j)d+=(a[j]-b[j])*(a[j]-b[j]);return std::sqrt(d);
}
int main() {
    std::array<uint8_t,80> vertices{};
    const std::array<std::array<float,3>,4> points{{{0,-2,-1},{0,-2,1},{0,2,1},{0,2,-1}}};
    for(unsigned i=0;i<4;++i){std::memcpy(vertices.data()+i*20,points[i].data(),12);
        for(unsigned j=12;j<20;++j)vertices[i*20+j]=uint8_t(i*20+j);}
    const auto original=vertices;
    Check(openshao::FacePortalQuad(vertices,{0,0,-10}),"edge-on portal ribbon faces the view");
    for(unsigned i=0;i<4;++i){auto p=Vertex(vertices,i);Check(std::abs(p[2])<1e-5,"visible ribbon width lies across the view");
        Check(std::memcmp(vertices.data()+i*20+12,original.data()+i*20+12,8)==0,"original color and packed UV are exact");
        for(unsigned k=i+1;k<4;++k)Check(std::abs(Distance(Vertex(original,i),Vertex(original,k))-Distance(p,Vertex(vertices,k)))<1e-5,"ribbon shape and taper are retained");}
    for(unsigned e=0;e<2;++e)for(unsigned j=0;j<3;++j)
        Check(std::abs(Vertex(original,e*2)[j]+Vertex(original,e*2+1)[j]-Vertex(vertices,e*2)[j]-Vertex(vertices,e*2+1)[j])<1e-5,"end centers remain fixed");
    const auto aligned=vertices;openshao::FacePortalQuad(vertices,{0,0,-10});Check(vertices==aligned,"already facing ribbons remain stable");
    std::array<uint8_t,80> zero{};Check(!openshao::FacePortalQuad(zero,{0,0,-10}),"degenerate ribbons are rejected");
    auto invalid=vertices;float nan=std::numeric_limits<float>::quiet_NaN();std::memcpy(invalid.data(),&nan,4);const auto saved=invalid;
    Check(!openshao::FacePortalQuad(invalid,{0,0,-10})&&invalid==saved,"non-finite geometry is not changed");
    Check(!openshao::FacePortalQuad(std::span<uint8_t>(vertices).first(79),{0,0,-10}),"partial guest records are rejected");
    std::array<float,16> matrix{0,0,-1,0, 0,1,0,0, 1,0,0,0, 100,200,300,1};
    auto transformed=original;
    for(unsigned i=0;i<4;++i){std::array<float,3> p;
        for(unsigned j=0;j<3;++j){p[j]=matrix[12+j];for(unsigned k=0;k<3;++k)p[j]+=points[i][k]*matrix[k*4+j];}
        std::memcpy(transformed.data()+i*20,p.data(),12);}
    Check(openshao::RestorePortalWorldAxes(transformed,matrix),"camera rotation is removed from authored world velocity");
    for(unsigned i=0;i<4;++i){auto p=Vertex(transformed,i);for(unsigned j=0;j<3;++j)Check(std::abs(p[j]-points[i][j]-matrix[12+j])<1e-5,"world velocity axes and particle translation are restored");
        Check(std::memcmp(transformed.data()+i*20+12,original.data()+i*20+12,8)==0,"world-axis restoration retains color and UV");}
    auto bad_matrix=matrix;bad_matrix[0]=2;const auto before_invalid=transformed;
    Check(!openshao::RestorePortalWorldAxes(transformed,bad_matrix)&&transformed==before_invalid,"non-rotation matrices are not applied");
    std::puts("PASS: portal orientation, shape, world axes and invalid-input guards");
}
