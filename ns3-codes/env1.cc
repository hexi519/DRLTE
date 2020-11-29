/*
Simulation environment for DRL-TE
V3.1
Time:2018/9/19
LOG standard:
NS_LOG_ERROR - error
NS_LOG_WARN - warning
NS_LOG_DEBUG - not common information
NS_LOG_INFO - usual information, set this level when simulation
NS_LOG_FUNCTION - print function calling information and session related information
NS_LOG_LOGIC - print topology construction information and packet related information
NS_LOG_ALL - all the information in sub-functions, no related function to LOG
Changes:
shutdown some prints
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>
#include <cassert>
#include <numeric>
#include <math.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
//#include <python3.5m/Python.h>
#include "ns3/socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/simulator.h"
#include "ns3/simple-channel.h"
#include "ns3/simple-net-device.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/log.h"
#include "ns3/config-store-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/queue.h"
#include "ns3/drop-tail-queue.h"
#include "ns3/ipv4-address.h"
#include "ns3/double.h"
#include "ns3/config.h"
#include "ns3/config-store-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3; 
using namespace std;

NS_LOG_COMPONENT_DEFINE ("DrlEnv");

typedef pair<int, int> Edge;
bool printEnable0;//init related
bool printEnable1;//packet related
bool enableFlag;//result related
fstream outputFile;//output plot data
bool burstFlag = false;
bool failureFlag = false;

class DrlRouting
{
public:

  DrlRouting();
  ~DrlRouting();

  void init();
  void addPathsFromFile();
  void addPath(vector<int> path, double ratio_);
  void initNodes();
  void initPaths();
  void initAddrs();
  void initStaticRoute();
  void initSocket();
  void initPyConnect(); 
  
  void begin();
  void randSendData(int iSession, uint8_t *data);
  void update();
  void update(vector<vector<int>> pacNos, vector<vector<double>> dels, vector<vector<double>> thrs, vector<vector<int>> ECNpkts, vector<pair<int,int>> srcEdges, vector<double> utilList, vector<double> lossList, double maxUtil, vector<int> netUtil, vector<vector<vector<int>>> sessPathUtil, vector<vector<int>> sessUtil);
  void saveToFile(vector<vector<int>> pacNos, vector<vector<double>> dels, vector<vector<double>> thrs, vector<vector<int>> ECNpkts);

  void setSendRate(vector<string> rates);
  void setCap(string rate);
  void setLinkDelay(string delay);
  void setUpTime(int time);
  void setStopTime(int time);
  void setPacketSize(int size);
  void setInputPath(string path);
  void setOutputPath(string path);
  void setErrp(double err);
  void setECNqThr(uint16_t thr);
  void setServerPort(int port);
  void randRatio();
  void meanRatio();
  int spsToEdge(int sess, int path, int step);
  int spsDir(int sess, int path, int step);
  int spsSrc(int sess, int path, int step);
  int spsEnd(int sess, int path, int step);
  string getAddr(unsigned int addr);

private:
  double err_p;
  double thrSum;
  double delSum;
  uint16_t ECNqThr;
  int upNum;
  string inputPath;
  string outputPath;
  int packetSize;
  int port;
  int server_port;
  int nodeNum;
  int topoNodeNum;
  int pathNum;
  int sessionNum;
  double stopTime;
  double lastSec;
  double upTime;
  uint8_t *data;
  int server_fd;
  int client_fd;

  DataRate cap;
  string linkDelay;
  vector<DataRate> sendRates;
  vector<double> sendRatesNoise;
  vector< vector< vector<int> > > sPaths;
  vector< vector< vector<int> > > ePaths;
  vector< vector<double> > sRatios;

  NodeContainer nodes;
  InternetStackHelper internet;
  PointToPointHelper p2p;
  //PointToPointHelper p2pB;//for bottle link
  //DataRate capB;//for bottle link
  //vector<int> bottleLink;//for bottle link
  //vector<int> bDevId;//for bottle link
  vector<Edge> edges;
  vector<NetDeviceContainer> spDevices;
  vector<NetDeviceContainer> spDevices2;//for maximum utilization compute
  Ptr<RateErrorModel> errModel;
  vector< vector< Ptr<NetDevice> > > slDevices;
  int srcTopoNode[1000][50];//at most 500 nodes     
  vector<pair<int,int>> srcEdges;
  vector<vector<uint32_t>> totalRxBytesV;
  vector<vector<uint32_t>> dropPacketsV;
  vector<vector<uint32_t>> totalPacketsV;
  vector<uint32_t> totalRxBytes4maxUtil;
  vector<uint32_t> sessSendPkts;
  vector<vector<uint32_t>> totalRxBytes4spUtil;
  
  Ipv4AddressHelper ipv4;
  Ipv4StaticRoutingHelper srHelper;
  vector< Ptr<Ipv4StaticRouting> > srNodes;
  vector<Ipv4InterfaceContainer> address;
  
  vector< vector< Ptr<Socket> > > srcsSocks;
  vector< vector< Ptr<Socket> > > endsSocks;
  vector< vector< Ptr<NetDevice> > > endsDevs;
  vector< vector< Ptr<NetDevice> > > srcsDevs;
  vector<ApplicationContainer> sinkApps;
};

double getRand();
int getRand(vector<double> ratio);
int indexOf(vector<Edge> container, Edge value);
string doubleToString(double d);
double stringToDouble(string str);
string intToString(int d);
int stringToInt(string str);
void UdpSend(Ptr<Socket> sock, Ptr<Packet> packet, int sess, int path);
void recvCallback(Ptr<Socket> socket);


//-----< main function >-----
int main(int argc, char *argv[])
{
  #if 1                                                       //for simulation
  LogComponentEnable ("DrlEnv", LOG_LEVEL_INFO);
  #elif 0                                                     //for logic debug
  LogComponentEnableAll (LOG_PREFIX_TIME);                    //print time before each log information
  LogComponentEnable ("DrlEnv", LOG_LEVEL_FUNCTION);
  LogComponentEnable ("Ipv4StaticRouting", LOG_LEVEL_ERROR);
  #else                                                       //for packet-level debug
  LogComponentEnableAll (LOG_PREFIX_TIME);
  LogComponentEnable ("DrlEnv", LOG_LEVEL_LOGIC);
  #endif

  string fileName = "inputKite";//input//1221SHR//NSF_s20_SHR_p3//inputKite

  int upTime=500;//ms
  int stopTime=16;//s
  int packetSize=1024;
  int serverPort=50001;
  double err_p=0.0;//?????0
  //for scale in
  //uint16_t ECNqThr=200;
  uint16_t ECNqThr=5;
  uint32_t seed=1;
  printEnable0=false;//debug now:remote server
  printEnable1=false;
  inputKite=true;//forinputKite socket now
  burstFlag=false;
  failureFlag=false;

  CommandLine cmd;
  cmd.AddValue ("filename", "input filename", fileName);
  cmd.AddValue ("uptime", "update interval time", upTime);
  cmd.AddValue ("stoptime", "stop time, seconds", stopTime);
  cmd.AddValue ("packetsize", "KB", packetSize);
  cmd.AddValue ("ecn", "ecn threshold", ECNqThr);
  cmd.AddValue ("serverport", "server port", serverPort);
  cmd.AddValue ("burstflag", "burst flag", burstFlag);  //! hesy :Set to be true
  cmd.AddValue ("failureflag", "failure flag", failureFlag);
  cmd.Parse(argc, argv);
  Config::SetDefault ("ns3::Queue::Mode", StringValue ("QUEUE_MODE_PACKETS"));
  Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
  //Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue(1000));
  //Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(100000000));
  //for scale in
  Config::SetDefault ("ns3::Queue::MaxPackets", UintegerValue(50));
  Config::SetDefault("ns3::UdpSocket::RcvBufSize", UintegerValue(10000000));

  SeedManager::SetSeed(seed);       //? difference
  SeedManager::SetRun(seed);

  string TXT = ".txt";
  string inputPath("./scratch/DRLTE/inputs/" + fileName + TXT);
  string outputPath("./scratch/DRLTE/outputs/" + fileName + TXT);
  //system("dos2unix ./scratch/DRLTE/inputs/*.txt");
//----------------------------------------------------------------
  NS_LOG_INFO("DrlRouting init begin!");
  DrlRouting drl=DrlRouting();
  drl.setServerPort(serverPort);
  drl.setErrp(err_p);
  drl.setECNqThr(ECNqThr);
  drl.setInputPath(inputPath);
  drl.setOutputPath(outputPath);
  drl.setUpTime(upTime);
  drl.setStopTime(stopTime);
  drl.setPacketSize(packetSize);
  drl.init();
  Simulator::Schedule(Seconds(5.0), &DrlRouting::begin, &drl);//begin after 5 seconds
//---------------------------------------------------------------- 

  Simulator::Run();
  Simulator::Destroy();
  return 0;
}

DrlRouting::DrlRouting()
{
  err_p=0.0;
  thrSum=0;//total throughput
  delSum=0;//total delay
  ECNqThr=200;
  upNum=0;//number of scheduling
  inputPath="";//the filepath of file for paths
  outputPath="";//the filepath of the file for plot
  packetSize=1024;
  port=12345;
  server_port=54321;
  nodeNum=0;
  topoNodeNum=0;
  pathNum=0;
  sessionNum=0;
  linkDelay="1ms";
  stopTime=100;//stop time of this simulation
  lastSec=0;
  upTime=500;//interval between two adjacent updates, i.e. schedulings
  data=new uint8_t[4000];//new a data block
  server_fd = -1;
  client_fd = -1;
  memset(srcTopoNode, 0, 50000);
}

DrlRouting::~DrlRouting()
{
  if(outputFile) outputFile.close();
  close(server_fd);
  delete []data;
  data = NULL;
  NS_LOG_INFO(std::endl<<"Simulation is end!");
}

/*tool functions:*/
int DrlRouting::spsToEdge(int sess, int path, int step)
{
  int temp=ePaths[sess][path][step];
  if(temp>=0) return temp-1;
  return -temp-1;
}

int DrlRouting::spsDir(int sess, int path, int step)
{
  int temp=ePaths[sess][path][step];
  if(temp>=0) return 1;
  return -1;
}

int DrlRouting::spsSrc(int sess, int path, int step)
{
  int temp=ePaths[sess][path][step];
  if(temp>=0) return 1;
  return 2;
}

int DrlRouting::spsEnd(int sess, int path, int step)
{
  int temp=ePaths[sess][path][step];
  if(temp>=0) return 2;
  return 1;
}

string DrlRouting::getAddr(unsigned int addr)
{
  std::ostringstream subset;
  unsigned int i = addr;
  if (i < 255) {
    subset << "10.0." << i + 1 << ".0";
  }
  else {
    subset << "10." << int(i/255) << "." << i - int(i / 255)*255 + 1 << ".0";
  }
  return subset.str();
}

double getRand()
{
  double f;
  f = (float)(rand() % 100);
  return f/100;
}
int getRand(vector<double> ratio)
{
  int max=1000;
  int len=ratio.size();
  vector<double> sec;
  double next=0.0;
  sec.push_back(next);
  for(int i=0; i<len-1; i++){
    next=next+ratio[i];
    sec.push_back(next*max);
  }
  sec.push_back(max);
  int res=rand()%(max+1);
  for(int i=0; i<len; i++){
    if(res>=sec[i] && res<=sec[i+1]) return i;
  }
  return -1;
}
int indexOf(vector<Edge> container, Edge value)
{
  for(unsigned int i=0; i<container.size(); i++)
    if(container[i]==value) return i;
  return -1;
}
string doubleToString(double d)
{
  std::ostringstream os;
  if(os<<d) return os.str();
  return "Error";
}

double stringToDouble(string str)
{
  std::istringstream is(str);
  double d; 
  if(is>>d) return d;
  return -9999999.99999;
}

string intToString(int d)
{
  std::ostringstream os;
  if(os<<d) return os.str();
  return "Error";
}

int stringToInt(string str)
{
  std::istringstream is(str);
  int d;
  if(is>>d) return d;
  return -9999999;
}
//*********************************************************************
/*initialization functions:*/
void DrlRouting::init()
{
  addPathsFromFile();
  initNodes();
  initPaths();
  initAddrs();
  initStaticRoute();
  initSocket();
  initPyConnect(); 
}
void DrlRouting::addPathsFromFile()
{
  NS_LOG_FUNCTION("addPathFromFile()");
  std::ifstream in(inputPath.c_str());
  if(!in.is_open()){
    NS_LOG_ERROR("Error opening file");
    exit(1);
  }
  char buffer[500];
  vector<vector<int>> res;
  in.getline(buffer, 500);
  string cap(buffer);
  cap.append("Mbps");//link capacity
  while(!in.eof()){ 
    in.getline(buffer, 500);
    if(buffer[0] == 's') break;//"succeed" means the end of paths
    string str(buffer);
    vector<int> vec;
    unsigned int start=0;
    unsigned int pos=0;
    do{ // process string via seperation of ","
      pos=str.find_first_of(',', start);//search the first ',' from start and return the index of ','
      int num=atoi(str.substr(start, pos-start).c_str());
      vec.push_back(num);
      start=pos+1;
    }while(pos<500);
    res.push_back(vec); // res restores path
  }
  in.getline(buffer, 500);//send rates of the sessions      //? 所以目前是两个session of 10Mbps?
  string str(buffer);
  NS_LOG_FUNCTION("session send rates: " << str);
  vector<string> v_sendRate;
  unsigned int start=0;
  unsigned int end=0;
  do {
    end=str.find_first_of(',', start);
    v_sendRate.push_back(str.substr(start, end-start).append("Mbps"));
    start=end+1;
  }while(end < str.size());
  //for bottle neck link
  /*
  in.getline(buffer, 500);
  string cap2(buffer);
  cap2.append("Mbps");
  capB = DataRate(cap2.c_str());
  in.getline(buffer, 500);
  string str2(buffer);
  start=0;
  end=0;
  //cout << buffer << endl;
  do {
    end=str2.find_first_of(',', start);
    string node = str2.substr(start, end-start);
    bottleLink.push_back(stringToInt(node.c_str()));
    start=end+1;
  }while(end < str2.size());
  for(unsigned int i =0;i<bottleLink.size();i++){
    cout << bottleLink[i];
    if(i%2==0) cout << "-";
    else cout << " ";
  }cout<<endl;
  */
  //end bottle neck link
  in.close();
  setSendRate(v_sendRate);//set sendRates in DrlRouting
  setCap(cap);//set cap in DrlRouting
  for(unsigned int i=0; i<res.size(); i++){//init sPaths and sRatios
      addPath(res[i], 0);
  }
  meanRatio();//init sRatios by splitting evenly, i.e. 1.0/pathNum
}
void DrlRouting::addPath(vector<int> pPath_, double ratio_)//init sPaths and sRatios  //not done
{
  int size=pPath_.size();
  if(size<2) return;//if path length is 1, return directly
  vector<int> path_;
  for(int i=0; i<size; i++) path_.push_back(pPath_[i]);     // 复制出来
  int iSrc=path_[0];
  int iEnd=path_[size-2];//why size-2?? The last node is host!!!
  for(unsigned int i=0; i<sPaths.size(); i++){  // skip here at first due to sPaths.size() is 0
    int pathNum_=sPaths[i].size();
    for(int j=0; j<pathNum_; j++){
      int nodeNum_=sPaths[i][j].size();
      if(sPaths[i][j][0]==iSrc && sPaths[i][j][nodeNum_-2]==iEnd){
        sPaths[i].push_back(path_);
        sRatios[i].push_back(ratio_);
        return;
      }
    }
  }
  vector<vector<int>> sPath;
  sPath.push_back(path_);   //?! 很奇怪，sPath明明只有一条...  还是需要debug一下
  sPaths.push_back(sPath);
  vector<double> sRatio;
  sRatio.push_back(ratio_);
  sRatios.push_back(sRatio);
}
void DrlRouting::initNodes()
{
  int maxNodei=0;//maximum node  id
  int maxTopoNodei=0;
  sessionNum=sPaths.size();
  for(int i = 0; i < sessionNum; i++){
    sendRatesNoise.push_back(0.0);
  }
  pathNum=0;
  for(int i=0; i<sessionNum; i++) {
    srcTopoNode[sPaths[i][0][1]][0] = 0;
    for(unsigned int j=0; j<sPaths[i].size(); j++){
      pathNum++;
      int l;
      for(l = 1; l <= srcTopoNode[sPaths[i][0][1]][0]; l++) {
        if(srcTopoNode[sPaths[i][0][1]][l] == sPaths[i][j][2]) break;
      }
      if(l > srcTopoNode[sPaths[i][0][1]][0]){
        srcTopoNode[sPaths[i][0][1]][0] += 1;
        srcTopoNode[sPaths[i][0][1]][l] = sPaths[i][j][2];
      }
      for(unsigned int k=0; k<sPaths[i][j].size(); k++) {
        if(sPaths[i][j][k]>maxNodei) maxNodei=sPaths[i][j][k];
        if(k>0 && k<sPaths[i][j].size()-1)
          if(sPaths[i][j][k]>maxTopoNodei) 
            maxTopoNodei=sPaths[i][j][k];
      }
    }
  }
  nodeNum=maxNodei+1;
  topoNodeNum=maxTopoNodei+1;
  for(int i=0;i<nodeNum;i++){
    Ptr<Node> node=CreateObject<Node>();//create node
    nodes.Add(node);
  }
  internet.Install(nodes);//install protocol stack
  vector<Ptr <NetDevice> > lDevices;
  for(int i=0;i<topoNodeNum+1;i++){
    slDevices.push_back(lDevices);
  }
  ////cout << "topoNodeNum: " << topoNodeNum << endl;
  return;
}

void DrlRouting::initPaths()
{
  NS_LOG_FUNCTION("initPaths()");
  p2p.SetDeviceAttribute("DataRate", DataRateValue(cap));//set link attributes
  p2p.SetChannelAttribute("Delay", StringValue(linkDelay));

  //for random bottle neck link
  //p2pB.SetDeviceAttribute("DataRate", DataRateValue(capB));//set link attributes
  //p2pB.SetChannelAttribute("Delay", StringValue(linkDelay));
  //vector<int> bLinkHelper;
  //for(unsigned int i=0;i<bottleLink.size();i++){
  //  bLinkHelper.push_back(bottleLink[i]*1000+bottleLink[i+1]);
  //  i++;
  //}
  errModel=CreateObject<RateErrorModel>(); 
  errModel->SetAttribute("ErrorRate", DoubleValue(err_p));//set error model of channel
  for(int i=0; i<sessionNum; i++){
    int pathNum_=sPaths[i].size();
    vector< vector<int> > sePaths;
    for(int j=0; j<pathNum_; j++){
      vector<int> pePaths;
      int stepNum_=sPaths[i][j].size()-1;
      for(int k=0; k<stepNum_; k++){
        int i1=sPaths[i][j][k];
        int i2=sPaths[i][j][k+1];
        int is=min(i1, i2);
        int ib=max(i1, i2);
        Edge edgePair(is, ib);
        int edgeNo=indexOf(edges, edgePair);
        int dir= i1<i2 ? 1 : -1;
        if(edgeNo>=0){
          pePaths.push_back((edgeNo+1)*dir);
          NS_LOG_LOGIC("session-:"<<i<<", path:"<<j<<", step:"<<k<<". edge:("<<i1<<"->"<<i2<<")="<<pePaths.back());
          continue;
        }
        pePaths.push_back((edges.size()+1)*dir);
        NS_LOG_LOGIC("session:"<<i<<", path:"<<j<<", step:"<<k<<". edge:("<<i1<<"->"<<i2<<")="<<pePaths.back());
        edges.push_back(edgePair);
        Ptr<Node> n1=nodes.Get(is);
        Ptr<Node> n2=nodes.Get(ib);
        NodeContainer pair=NodeContainer(n1, n2);
        //for bottleneck link
        //unsigned int bLinkNum = 0;
        //for(bLinkNum=0; bLinkNum<bLinkHelper.size(); bLinkNum++){
        //  if(bLinkHelper[bLinkNum] == (is*1000+ib)) break;
        //}
        NetDeviceContainer step;
        //if(bLinkNum<bLinkHelper.size()){
        //  step=p2pB.Install(pair);
       //   cout << "installed bottleneck link:" << is << "-" << ib << endl;
        ///  bDevId.push_back(spDevices2.size());//for maximum util
        //}
        //else
          step=p2p.Install(pair);
        step.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(errModel));
        step.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(errModel));
        spDevices.push_back(step);
        if(ib < topoNodeNum){
          //cout << is << " " << ib << endl;
          spDevices2.push_back(step);
        }
        if (srcTopoNode[i1][0] > 0) {
          int l;
          for(l = 1; l <= srcTopoNode[i1][0]; l++) {
            if(srcTopoNode[i1][l] == i2) break;
          }
          if(l <= srcTopoNode[i1][0]) {
            Ptr<NetDevice> nd = (i1==is?step.Get(0):step.Get(1));
            slDevices[i1].push_back(nd);
          }
        }
        if (srcTopoNode[i2][0] > 0) {
          int l;
          for(l = 1; l <= srcTopoNode[i2][0]; l++) {
            if(srcTopoNode[i2][l] == i1) break;
          }
          if(l <= srcTopoNode[i2][0]) {
            Ptr<NetDevice> nd = (i2==is?step.Get(0):step.Get(1));
            slDevices[i2].push_back(nd);
          }
        }

      }
      sePaths.push_back(pePaths);
    }
    ePaths.push_back(sePaths);
  }
  return;
}

void DrlRouting::initAddrs()
{
  NS_LOG_FUNCTION("initAddrs()");
  for(unsigned int i=0; i<spDevices.size(); i++){
    string addr=getAddr(i);
    ipv4.SetBase(addr.c_str(), "255.255.255.0");
    address.push_back(ipv4.Assign(spDevices[i]));
    NS_LOG_LOGIC("edge "<<i<<" small node:"<<address.back().GetAddress(0)<<"big node:"<<address.back().GetAddress(1));
  }
  return;
}
void DrlRouting::initStaticRoute()  //! 设置静态路由
{
  NS_LOG_FUNCTION("initStaticRoute()");
  for(int i=0; i<nodeNum; i++){
    Ptr<Ipv4> ipv4Node=nodes.Get(i)->GetObject<Ipv4>();
    Ptr<Ipv4StaticRouting> srNode=srHelper.GetStaticRouting(ipv4Node);
    srNodes.push_back(srNode);
  }
  for(int i=0; i<sessionNum; i++)
  {
    int pathNum_=sPaths[i].size();
    for(int j=0; j<pathNum_; j++){
      int stepNum_=sPaths[i][j].size()-1;
      Ipv4Address addrEnd=address[spsToEdge(i, j, stepNum_-1)].GetAddress(spsEnd(i, j, stepNum_-1)-1);//destination address
      Ipv4Address addrSrc=address[spsToEdge(i, j, 0)].GetAddress(spsSrc(i, j, 0)-1);//source address
      for(int k=0; k<stepNum_; k++){
        int nodei=sPaths[i][j][k];
        int nodeiNext=sPaths[i][j][k+1];
        Ptr<Ipv4> ipv4Node=nodes.Get(nodei)->GetObject<Ipv4>();
        Ptr<Ipv4> ipv4NodeNext=nodes.Get(nodeiNext)->GetObject<Ipv4>();
        Ipv4Address addr=address[spsToEdge(i, j, k)].GetAddress(spsSrc(i, j, k)-1);
        Ipv4Address addrNext=address[spsToEdge(i, j, k)].GetAddress(spsEnd(i, j, k)-1);
        int itf=ipv4Node->GetInterfaceForAddress(addr);
        int itfNext=ipv4NodeNext->GetInterfaceForAddress(addrNext);
        NS_LOG_LOGIC("addrSrc:"<<addrSrc<<", end addr:"<<addrEnd<<", cur addr:"<<addr<<", next hop:"<<addrNext);
        srNodes[nodei]->AddHostRouteTo(addrEnd, addrNext, itf);//paras:destination address; next hop address; interface index; Metric of route in case of multiple routes to same destination
        srNodes[nodeiNext]->AddHostRouteTo(addrSrc, addr, itfNext);
      }
    }
  }
  return;
}
void DrlRouting::initSocket()
{
  NS_LOG_FUNCTION("initSocket()");
  
  for(int i=0; i<sessionNum; i++)
  {
    int pathNum_=sPaths[i].size();
    vector<Ptr<Socket>> srcSocks;
    vector<Ptr<Socket>> endSocks;
    for(int j=0; j<pathNum_; j++){
      int stepNum_=sPaths[i][j].size()-1;
      Ipv4Address addrEnd=address[spsToEdge(i, j, stepNum_-1)].GetAddress(spsEnd(i, j, stepNum_-1)-1);//destination address
      Ipv4Address addrSrc=address[spsToEdge(i, j, 0)].GetAddress(spsSrc(i, j, 0)-1);//source address
      int iSrc=sPaths[i][j][0];//source node
      int iEnd=sPaths[i][j].back();//destination node

      Ptr<Socket> srcSock=Socket::CreateSocket(nodes.Get(iSrc), TypeId::LookupByName("ns3::UdpSocketFactory"));
      Ptr<Socket> endSock=Socket::CreateSocket(nodes.Get(iEnd), TypeId::LookupByName("ns3::UdpSocketFactory"));
      InetSocketAddress beg=InetSocketAddress(addrSrc, ++port);
      srcSock->Bind(beg);
      InetSocketAddress dst=InetSocketAddress(addrEnd, port);
      endSock->Bind(dst);
      endSock->SetRecvCallback(MakeCallback(&recvCallback));//call a function when recieving a packet       //TODO! 主要是这里
      srcSock->Connect(dst);
      //endSock->Connect(beg);
      Ptr<Packet> packet=Create<Packet>(1024);//test packet
      Simulator::Schedule(Seconds(1.0), &UdpSend, srcSock, packet, i, j); //TODO! 主要是这里
      srcSocks.push_back(srcSock);
      endSocks.push_back(endSock);
    }
    NS_LOG_LOGIC("session "<<i<<" connected");
    endsSocks.push_back(endSocks);
    srcsSocks.push_back(srcSocks);
  }
  return;
}
void DrlRouting::initPyConnect()
{
  if(!enableFlag) return;
  //server_fd client_fd
  int server_port = this->server_port;///54321;
  int lisNum = 1;
  socklen_t len;
  struct sockaddr_in my_addr, their_addr; 

  if ((server_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) {
    NS_LOG_ERROR("Create socket failed!");  
    exit(1);  
  }else  
    NS_LOG_INFO("Socket created");  

  bzero(&my_addr, sizeof(my_addr));
  my_addr.sin_family = PF_INET;    //IPv4  
  my_addr.sin_port = htons(server_port);
  my_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(server_fd, (struct sockaddr *) &my_addr, sizeof(struct sockaddr)) == -1){
    NS_LOG_ERROR("Bind error!");  
    exit(1);  
  } else  
    NS_LOG_INFO("Binded");  

  if (listen(server_fd, lisNum) == -1) {  
    NS_LOG_ERROR("Listen error!");  
    exit(1);  
  } else  
    NS_LOG_INFO("Begin listening");
  
  
  len = sizeof(struct sockaddr); 
  //return;
  while (1) {
  if ((client_fd = accept(server_fd, (struct sockaddr *) &their_addr, &len)) == -1) {  
      NS_LOG_ERROR("Accept error!");
      exit(1);  
  }else {
    NS_LOG_INFO("drl connected");
    break;
    }
  }//while end
  //send session info
  char sendbuf[4000] = {0};
  string msg = "";
  int bufpos = 0;
  for(int i=0; i<sessionNum; i++) {
    int pn = sPaths[i].size();
    int src = sPaths[i][0][1];
    msg += intToString(src) + ',' + intToString(pn);
    if(i < sessionNum -1) msg += ',';
  }
  msg+=";";
  memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  //send network utilization
  int spdNum = 2*spDevices2.size();
  msg = "";
  msg += intToString(spdNum);
  msg += ";";
  memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  //send src edges info
  for(int i = 0; i < topoNodeNum; i++) {
    int ndNum = slDevices[i].size();
    msg = "";
    msg += intToString(i) + " ";
    msg += intToString(ndNum);
    if (i < topoNodeNum -1) {
      msg += ",";
      memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  //send session path link util info
  vector<vector<int>> sessPathUtilNum;
  vector<int> sessUtilNum;
  for(int i=0; i<sessionNum; i++){
    int pathNum_=sPaths[i].size();
    vector<int> sesspathutilnum;
    int sessutilnum = 0;
    vector<Edge> sessEdge;
    for(int j=0; j<pathNum_; j++){
      int pathutilnum = 0;
      int stepNum_=sPaths[i][j].size()-1;
      for(int k=1; k<stepNum_-1; k++){
        int dirEdge = ePaths[i][j][k];
        int edgeNo;
        if(dirEdge > 0){
          edgeNo = dirEdge - 1;
        }else{
          edgeNo = -1*dirEdge - 1;
        }
        pathutilnum += 1;
        int edgeno=indexOf(sessEdge, edges[edgeNo]);
        if(edgeno < 0){
          sessEdge.push_back(edges[edgeNo]);
          sessutilnum += 1;
        }
        
      }
      sesspathutilnum.push_back(pathutilnum);
    }
    sessPathUtilNum.push_back(sesspathutilnum);
    sessUtilNum.push_back(sessutilnum);
  }
  //write
  for (int i = 0; i < sessionNum; i++) {
    msg = "";
    int pn = sessPathUtilNum[i].size();
    for (int j = 0; j < pn; j++) {
      msg += intToString(sessPathUtilNum[i][j]);
      if(j < pn-1) msg += " ";
    }
    if (i < sessionNum -1) {
      msg += ",";
      memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  //write2
  msg = "";
  for (int i = 0; i < sessionNum; i++) {
    msg += intToString(sessUtilNum[i]);
    if (i < sessionNum -1) {
      msg += ",";
    }
  }
  msg += ";";
  memcpy(sendbuf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();

  


/*for msg total length*/
  msg = "";
  msg = intToString(bufpos) + ";";
  char Msg[4000];
  memset(Msg, 0, 4000);
  memcpy(Msg, msg.c_str(), msg.length());
  memcpy(Msg + msg.length(), sendbuf, bufpos);
  bufpos += msg.length();
  ////cout << endl << bufpos << endl << Msg << endl;

  send(client_fd, Msg, bufpos, 0);
}
//********************************************************
/*scheduling functions*/
void DrlRouting::begin()
{
  std::ofstream file;
  file.open(outputPath.c_str(), ios::trunc);//if the file has already existed, then delete it
  if(file) file.close();
  outputFile.open(outputPath.c_str(), std::ios::app);
  if(!outputFile){
    NS_LOG_ERROR(outputPath<<" can't open");
    exit(1);
  }
  
  lastSec=Simulator::Now().GetSeconds();
  NS_LOG_INFO(std::endl<<"begin(), time:"<<lastSec<<"s, stopTime:"<<stopTime<<"s");
  for(int i=0; i<sessionNum; i++){
    for(unsigned int j=0; j<sPaths[i].size(); j++){
      endsSocks[i][j]->thrs.clear();
      endsSocks[i][j]->dels.clear();
      endsSocks[i][j]->qSizes.clear();
    }
    sessSendPkts.push_back(0);
    Simulator::ScheduleNow(&DrlRouting::randSendData, this, i, data);
  }
}

void DrlRouting::randSendData(int iSession, uint8_t *data)//size is the total amount in this round
{
  Ptr<ExponentialRandomVariable> ExpDstrib=CreateObject<ExponentialRandomVariable>();
  Ptr<UniformRandomVariable> UniDstrib=CreateObject<UniformRandomVariable>();
  
  double time=Simulator::Now().GetSeconds();
  int iPath=getRand(sRatios[iSession]);//select a path
  int realPacSize = packetSize;
  Ptr<Packet> packet=Create<Packet>(data, realPacSize);
  sessSendPkts[iSession] += 1;
  #if 1//my way
  double dataRate_b=sendRates[iSession].GetBitRate();
  //for flow change test 2018.9.18
  if(burstFlag && (time <= 9000)){
    dataRate_b = dataRate_b*(1.0 + sendRatesNoise[iSession]);
    //dataRate_b = dataRate_b*(1.2);
  }
  if(dataRate_b==0) return;
  double unit_t = 0.001;//s
  double lamda = dataRate_b*unit_t/(realPacSize*8);
  double micros=1000*ExpDstrib->GetValue(1/lamda, 1/lamda*5);
  
  #elif 0//author's method
  double dataRate_mb=sendRates[iSession].GetBitRate()/1000000;
  double micros=(double)realPacSize*8/dataRate_mb;
  int ns=micros*1000;
  ns=ExpDstrib->GetInteger(ns, ns*1.2);//??????
  micros=(double)ns/1000.0;
  #endif
  
  Time tNext(MicroSeconds(micros));
  Simulator::ScheduleNow(&UdpSend, srcsSocks[iSession][iPath], packet, iSession, iPath);
  if(time-lastSec>=upTime/1000){
    lastSec=time;
    update();
  }
  
  if(stopTime >= 5 && Simulator::Now().GetSeconds() <= stopTime+0.1){//begin time is at t=5s, so stoptime should be later than t=5s
    Simulator::Schedule(tNext, &DrlRouting::randSendData, this, iSession, data);
  }
  return;
}

void DrlRouting::update()
{
  upNum++;
  if(burstFlag && (upNum % 2000 == 1)){
    cout << "ssss" << endl;
    for(unsigned int i = 0; i < sendRates.size(); i++){
      double noise = 2*(getRand()*0.1 - 0.05);
      sendRatesNoise[i] = noise;
      cout << noise << endl;
    }
  }
  double time=Simulator::Now().GetSeconds();
  NS_LOG_INFO(std::endl<<"update(), time:"<<time);
  vector<vector<double>> spDels;
  vector<vector<double>> spThrs;
  vector<vector<int>> spECNpktNums;
  vector<vector<int>> spPacNos;
  vector<vector<double>> linkUtil;
  for(int i=0; i<sessionNum; i++){
    int pathNum=sPaths[i].size();
    vector<double> dels;
    vector<double> thrs;
    vector<int> ECNpktNums;
    vector<int> pacNos;
    for(int j=0; j<pathNum; j++){
      double del=0.0;
      double thr=0.0;
      int ECNpktNum = 0;
      int pacNo=endsSocks[i][j]->dels.size();
      for(int k=0; k<pacNo; k++){
        double del_=endsSocks[i][j]->dels[k];
        if(del_<=0) {pacNo--; continue;}
        del+=del_;
        thr+=endsSocks[i][j]->thrs[k];
        if (endsSocks[i][j]->qSizes[k] > ECNqThr)
          ECNpktNum++;
      }
      if(pacNo>0) del=del/(double)pacNo;
      thr=thr/upTime*1000*8;//KB->bps
      thr=thr/1000/1000;//bps->Mbps;
      pacNos.push_back(pacNo);
      dels.push_back(del);
      thrs.push_back(thr);
      ECNpktNums.push_back(ECNpktNum);
      if(false) NS_LOG_FUNCTION("session:"<<i<<", path:"<<j<<", pacNo:"<<pacNo<<", thr:"<<thr<<", del:"<<del << ", ECNpktNum:" << ECNpktNum);
      endsSocks[i][j]->thrs.clear();
      endsSocks[i][j]->dels.clear();
      endsSocks[i][j]->qSizes.clear();
    }
    spDels.push_back(dels);
    spThrs.push_back(thrs);
    spECNpktNums.push_back(ECNpktNums);
    spPacNos.push_back(pacNos);
  }
  /////for utilization calculate

  Ptr<NetDevice> nd;
  Ptr<PointToPointNetDevice> p2pnd;
  Ptr<Queue> queue;
  uint32_t totalDropBytes, totalRxBytes, totalBytes;
  uint32_t totalDropPackets, totalRxPackets, totalPackets;


  ///calculate session-based path-based link utilization
  vector<vector<uint32_t>> totalRxBytes4spUtiltmp;
  int devNum0 = spDevices.size();
  if(upNum==1){
    for(int i=0; i<devNum0; i++){
      vector<uint32_t> tmp(2);
      totalRxBytes4spUtil.push_back(tmp);
    }
  }
  totalRxBytes4spUtiltmp = totalRxBytes4spUtil;
  vector<vector<vector<int>>> sessPathUtil;
  vector<vector<int>> sessUtil;
  for(int i=0; i<sessionNum; i++){
    int pathNum_=sPaths[i].size();
    vector<vector<int>> sesspathutil;
    vector<int> sessutil;
    vector<Edge> sessEdge;
    for(int j=0; j<pathNum_; j++){
      vector<int> pathutil;
      int stepNum_=sPaths[i][j].size()-1;
      for(int k=1; k<stepNum_-1; k++){
        int dirEdge = ePaths[i][j][k];
        int edgeNo, portNo;
        double util;
        if(dirEdge > 0){
          edgeNo = dirEdge - 1;
          portNo = 0;
        }else{
          edgeNo = -1*dirEdge - 1;
          portNo = 1;
        }
        nd = spDevices[edgeNo].Get(portNo);
        p2pnd = StaticCast<PointToPointNetDevice> (nd);
        queue = p2pnd->GetQueue();
        totalRxBytes = queue->GetTotalReceivedBytes();
        util = ((double)(totalRxBytes-totalRxBytes4spUtil[edgeNo][portNo])*8*2)/cap.GetBitRate();
        totalRxBytes4spUtiltmp[edgeNo][portNo] = totalRxBytes;

        pathutil.push_back(int(util*10000));
        int edgeno=indexOf(sessEdge, edges[edgeNo]);
        if(edgeno < 0){
          sessEdge.push_back(edges[edgeNo]);
          sessutil.push_back(int(util*10000));
        }
        
      }
      sesspathutil.push_back(pathutil);
    }
    sessPathUtil.push_back(sesspathutil);
    sessUtil.push_back(sessutil);
  }
  totalRxBytes4spUtil = totalRxBytes4spUtiltmp;
  ////calculate every link utilization
  int devNum = spDevices2.size();
  double util, loss;
  vector<double> utilList, lossList;

  double maxUtil = 0.0;
  int k = 0;
  if(upNum==1){
    for(int i=0; i<devNum*2; i++)
      totalRxBytes4maxUtil.push_back(0);
  }
  //for failure test
  //if(failureFlag && time >= 1006){
    //failureFlag = false;
    //int failDev=rand()%(devNum);
    //spDevices[failDev]
  //}
  //failure test end
  //cout << "---" << spDevices2.size() << "------" << spDevices.size() << endl;
  //capB
  vector<int> netUtil;
  for(int i = 0; i < devNum; i++) {
    double capa = 0;
    //if(0 == std::count(bDevId.begin(), bDevId.end(), i)){
      capa = cap.GetBitRate();
    //}else{
      //capa = capB.GetBitRate();
      //cout << "capa:" << i << endl;
    //}
    nd = spDevices2[i].Get(0);
    p2pnd = StaticCast<PointToPointNetDevice> (nd);
    queue = p2pnd->GetQueue();
    totalRxBytes = queue->GetTotalReceivedBytes();
    util = ((double)(totalRxBytes-totalRxBytes4maxUtil[k])*8*2)/capa;
    totalRxBytes4maxUtil[k] = totalRxBytes;
    k++;
    ////cout << "util:" << util << endl;
    maxUtil = maxUtil > util?maxUtil:util;
    netUtil.push_back(int(util*10000));


    nd = spDevices2[i].Get(1);
    p2pnd = StaticCast<PointToPointNetDevice> (nd);
    queue = p2pnd->GetQueue();
    totalRxBytes = queue->GetTotalReceivedBytes();
    util = ((double)(totalRxBytes-totalRxBytes4maxUtil[k])*8*2)/capa;
    totalRxBytes4maxUtil[k] = totalRxBytes;
    k++;
    ////cout << "util:" << util << endl;
    maxUtil = maxUtil > util?maxUtil:util;
    netUtil.push_back(int(util*10000));
  }
  cout << "maxUtil:" << maxUtil << endl;
  #if 0
  for(int i = 0; i < nodeNum; i++) {
    cout << "hear0" << endl;
    for(uint32_t j = 1; j < nodes.Get(i)->GetNDevices(); j++) {//loopback interface should not be counterd hear
      nd = nodes.Get(i)->GetDevice(j);
      p2pnd = StaticCast<PointToPointNetDevice> (nd);
      queue = p2pnd->GetQueue();
      totalDropBytes = queue->GetTotalDroppedBytes();
      totalRxBytes = queue->GetTotalReceivedBytes();
      totalBytes = totalDropBytes + totalRxBytes;
      cout << "---------" << endl;
      cout << "totalDropBytes:" << totalDropBytes << " totalRxBytes:" << totalRxBytes << " totalBytes:" << totalBytes << endl;
      totalDropPackets = queue->GetTotalDroppedPackets();
      totalRxPackets = queue->GetTotalReceivedPackets();
      totalPackets = totalDropPackets + totalRxPackets;
      cout << "totalDropPackets:" << totalDropPackets << " totalRxPackets:" << totalRxPackets << " totalPackets:" << totalPackets << endl;
      util = ((double)totalRxBytes*8*2)/cap.GetBitRate();
      loss = ((double)totalDropBytes)/totalBytes;
      cout << "util:" << util << " loss:" << loss << endl;
    }
  }
  #endif
  ////calculate link utilizations for links of each node
  for(int i = 0; i < topoNodeNum; i++) {
    ////cout << "src node: " << i << endl;
    int ndNum = slDevices[i].size();
    if(upNum == 1) {
      vector<uint32_t> tmpv;
      for(int k=0; k < ndNum; k++){
        tmpv.push_back(0);
      }
      totalRxBytesV.push_back(tmpv);
      dropPacketsV.push_back(tmpv);
      totalPacketsV.push_back(tmpv);
      pair<int,int> tmpp;
      tmpp.first = i;
      tmpp.second = ndNum;
      srcEdges.push_back(tmpp);
    }
    if(ndNum == 0) continue;
    for(int j = 0; j < ndNum; j++) {
      nd = slDevices[i][j];
      p2pnd = StaticCast<PointToPointNetDevice> (nd);
      queue = p2pnd->GetQueue();
      totalDropBytes = queue->GetTotalDroppedBytes();
      totalRxBytes = queue->GetTotalReceivedBytes();
      totalBytes = totalDropBytes + totalRxBytes;
      if(false)
      ////cout << "---------" << endl;
        cout << "totalDropBytes:" << totalDropBytes << " totalRxBytes:" << totalRxBytes << " totalBytes:" << totalBytes << endl;
      totalDropPackets = queue->GetTotalDroppedPackets();
      totalRxPackets = queue->GetTotalReceivedPackets();
      totalPackets = totalDropPackets + totalRxPackets;
      ////cout << "totalDropPackets:" << totalDropPackets << " totalRxPackets:" << totalRxPackets << " totalPackets:" << totalPackets << endl;
      util = ((double)(totalRxBytes-totalRxBytesV[i][j])*8*2)/cap.GetBitRate();
      if(totalPackets == totalPacketsV[i][j]) 
        loss = ((double)(totalDropPackets-dropPacketsV[i][j]))/(totalPackets-totalPacketsV[i][j]);
      else loss = 0.0;
      ////cout << "util:" << util << " loss:" << loss << endl;
      utilList.push_back(util);
      lossList.push_back(loss);
      totalRxBytesV[i][j] = totalRxBytes;
      dropPacketsV[i][j]=totalDropPackets;
      totalPacketsV[i][j]=totalPackets;
    }
  }


  //saveToFile(spPacNos, spDels, spThrs, spECNpktNums); //comment at 2018.8.30
  update(spPacNos, spDels, spThrs, spECNpktNums, srcEdges, utilList, lossList, maxUtil, netUtil, sessPathUtil, sessUtil);
  #if 0 // to see the pachet loss of each session
  cout << "sessSendPkts:" << endl;
  for(unsigned int i=0; i < sessSendPkts.size(); i++){
    cout << sessSendPkts[i] << "(";
    sessSendPkts[i] = 0;
    int pathNum=sPaths[i].size();
    uint32_t pktTotal = 0;
    for(int j=0; j<pathNum; j++){
      pktTotal += spPacNos[i][j];
    }
    cout << pktTotal << ") ";
  }
  cout << endl;
  #endif
  //randRatio();
  //if(upNum%20==0) updateWindow();
}

void DrlRouting::update(vector<vector<int>> pacNos, vector<vector<double>> dels, vector<vector<double>> thrs, vector<vector<int>> ECNpkts, 
  vector<pair<int,int>> srcEdges, vector<double> utilList, vector<double> lossList, double maxUtil, vector<int> netUtil, vector<vector<vector<int>>> sessPathUtil, vector<vector<int>> sessUtil)
{
  if (!enableFlag) return;
  NS_LOG_FUNCTION("communicate with drl server");
  if (Simulator::Now().GetSeconds() > stopTime - 0.5) {
      char sendbuf[1024] = "0;updatefinished";
      send(client_fd, sendbuf, 1024, 0);
      cout << "update finished!" << endl;
      return;
  }
  /*construct Msg buffer*/
  int BUFSIZE = 30000;
  char buf[BUFSIZE];
  memset(buf, 0, BUFSIZE);
  int bufpos = 0;
  string msg = "";
  int sn = sessionNum;
  /*for pacNos*/
  for (int i = 0; i < sn; i++) {
    msg = "";
    int pn = pacNos[i].size();
    for (int j = 0; j < pn-1; j++) {
      msg += intToString(pacNos[i][j]) + " ";
    }
    msg += intToString(pacNos[i][pn-1]);
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for dels*/
  for (int i = 0; i < sn; i++) {
    msg = "";
    int pn = dels[i].size();
    for (int j = 0; j < pn-1; j++) {
      msg += doubleToString(dels[i][j]) + " ";
    }
    msg += doubleToString(dels[i][pn-1]);
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for thrs*/
  for (int i = 0; i < sn; i++) {
    msg = "";
    int pn = thrs[i].size();
    for (int j = 0; j < pn-1; j++) {
      msg += doubleToString(thrs[i][j]) + " ";
    }
    msg += doubleToString(thrs[i][pn-1]);
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for ECN pkts*/
  for (int i = 0; i < sn; i++) {
    msg = "";
    int pn = ECNpkts[i].size();
    for (int j = 0; j < pn-1; j++) {
      msg += intToString(ECNpkts[i][j]) + " ";
    }
    msg += intToString(ECNpkts[i][pn-1]);
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for srcEdges*/
  uint16_t srcNodesNum = srcEdges.size();
  for (uint16_t i = 0; i < srcNodesNum; i++) {
    msg = "";
    msg += intToString(srcEdges[i].first) + " ";
    msg += intToString(srcEdges[i].second);
    if (i < srcNodesNum -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for utilList and lossList*/
  uint16_t edgesNum = utilList.size();
  for (uint16_t i = 0; i < edgesNum; i++) {
    msg = "";
    msg += doubleToString(utilList[i]) + " ";
    msg += doubleToString(lossList[i]);
    if (i < edgesNum -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for maxutil*/
  msg = "";
  msg += doubleToString(maxUtil);
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for network utilization*/
  uint16_t netUtilLen = netUtil.size();
  for (uint16_t i = 0; i < netUtilLen; i++) {
    msg = "";
    msg += intToString(netUtil[i]);
    if (i < netUtilLen -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  /*for session path link utilization*/
  ///vector<vector<vector<int>>> sessPathUtil,
  for (int i = 0; i < sn; i++) {
    msg = "";
    int pn = sessPathUtil[i].size();
    for (int j = 0; j < pn; j++) {
      int ln = sessPathUtil[i][j].size();
      for(int k = 0; k < ln; k++) {
        msg += intToString(sessPathUtil[i][j][k]);
        if(k < ln-1) msg += "-";
      }
      if(j < pn-1) msg += " ";
    }
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();
  ///vector<vector<int>> sessUtil.
  for (int i = 0; i < sn; i++) {
    msg = "";
    int ln = sessUtil[i].size();
    for (int j = 0; j < ln; j++) {
      msg += intToString(sessUtil[i][j]);
      if(j < ln-1) msg += " ";
    }
    if (i < sn -1) {
      msg += ",";
      memcpy(buf + bufpos, msg.c_str(), msg.length());
      bufpos += msg.length();
    }
  }
  msg += ";";
  memcpy(buf + bufpos, msg.c_str(), msg.length());
  bufpos += msg.length();


  /*for msg total length*/
  msg = "";
  msg = intToString(bufpos) + ";";
  char Msg[BUFSIZE];
  memset(Msg, 0, BUFSIZE);
  memcpy(Msg, msg.c_str(), msg.length());
  memcpy(Msg + msg.length(), buf, bufpos);
  bufpos += msg.length();
  //////cout << endl << bufpos << endl << Msg << endl;
  /*construct Msg buffer end*/
  
  //return;
  /**************************/
  /*send Msg to drl server*/
  int msgLen = 0;
  int blockSize = 1024;//1024;
  int blockNum = (int)((blockSize - 1 + bufpos)/blockSize);
  for (int i = 0; i < blockNum; i++) {
    char sendbuf[blockSize] = {0};
    if (i < blockNum -1) {
      memcpy(sendbuf, Msg + i*blockSize, blockSize);
      msgLen = send(client_fd, sendbuf, blockSize, 0);
    }
    else {
      memcpy(sendbuf, Msg + i*blockSize, bufpos-blockSize*i);
      msgLen = send(client_fd, sendbuf, bufpos-blockSize*i, 0);
    }
    
    //cout << "msgLen:" << msgLen << endl;
  }
  
  /**************************/
  /*recv data from drl server*/
  char recvbuf[blockSize];
  unsigned int msgTotalLen = 0;
  unsigned int msgRecvLen = 0;
  unsigned int msgContLen = 0;
  memset(Msg, 0, BUFSIZE);
  bzero(Msg, BUFSIZE);
  while (1) {
    msgLen = recv(client_fd, recvbuf, blockSize, 0);
    if (msgLen > 0){
    //printf("msgrecv:%s\n", recvbuf);
      if(msgTotalLen == 0) {
        string strTmp = recvbuf;
        int posTmp = strTmp.find_first_of(';', 0);
        msgContLen = atoi(strTmp.substr(0, posTmp).c_str());
        msgTotalLen= msgContLen + (strTmp.substr(0, posTmp)).length() + 1;//1 is the length of ';'
        //cout << msgTotalLen << "  sdsssss " << msgContLen << endl;
      }
      memcpy(Msg+msgRecvLen, recvbuf, sizeof(recvbuf));
      msgRecvLen += sizeof(recvbuf);
      if (msgRecvLen < msgTotalLen)
        continue;
      NS_LOG_FUNCTION(Msg);
      break;
    }
  }
  /**************************/
  /*parse data and change session ratios: sRatios*/
  string strMsg(Msg);
  unsigned int start = strMsg.find_first_of(';', 0) + 1;
  unsigned int end = 0;
  string subStrMsg = strMsg.substr(start, msgContLen);
  start = 0;
  if(printEnable0) cout << "subStrMsg:" << subStrMsg << endl;
  vector<double> ratios;
  do {
    end = subStrMsg.find_first_of(',', start);
    string tmp = subStrMsg.substr(start, end-start);
    ratios.push_back(stringToDouble(tmp));
    start = end+1;
  }while(end < subStrMsg.size());
  #if 1
  int k = 0;
  for(int i = 0;i < sessionNum; i++)
    for(unsigned int j=0; j < sRatios[i].size(); j++) 
      sRatios[i][j] = ratios[k++];
  #endif
}

void DrlRouting::saveToFile(vector<vector<int>> pacNos, vector<vector<double>> dels, vector<vector<double>> thrs, vector<vector<int>> ECNpkts)
{
  double time=Simulator::Now().GetSeconds();
  
  NS_LOG_FUNCTION("saveToFile()");

  vector<double> sesthr;
  vector<double> sesdel;
  vector<double> seslog;
  int sn=sessionNum;
  for(int i=0; i<sn; i++){//sn:session No.
    int pn=sRatios[i].size();
    vector <double>::iterator Iter1;
    double thrSum=accumulate(thrs[i].begin(), thrs[i].end(), 0.0);//set 0.0, then double, else int
    int pacNoSum=accumulate(pacNos[i].begin(), pacNos[i].end(), 0);
    int ECNpktSum = accumulate(ECNpkts[i].begin(), ECNpkts[i].end(), 0);
    double delAve=0;
    double ECNratio = (double)(ECNpktSum)/(double)(pacNoSum);
    if(pacNoSum>0) for(int j=0; j<pn; j++) delAve+=dels[i][j]*pacNos[i][j]/pacNoSum;
    if(false) NS_LOG_FUNCTION("session "<<i<<",thrSum:"<<thrSum<<"Mbps, aveDel:"<<delAve<<"s, ECNratio:"<<ECNratio);
    sesthr.push_back(thrSum);
    sesdel.push_back(delAve);
    if(thrSum>0 && delAve >0) seslog.push_back(log(thrSum)-log(delAve));
    else seslog.push_back(0);
  }
  outputFile<<"--------------------------------------"<<std::endl;
  outputFile<<"Time: "<<doubleToString(time)<<std::endl;
  outputFile<<"Split Ratios: ";
  for(unsigned int i=0; i<sRatios.size(); i++)
    for(unsigned int j=0; j<sRatios[i].size(); j++) outputFile<<doubleToString(sRatios[i][j])<<" ";
  outputFile<<std::endl;
  outputFile<<"Delays: ";
  for(unsigned int i=0; i<sesdel.size(); i++) outputFile<<doubleToString(sesdel[i])<<" ";
  outputFile<<std::endl;
  outputFile<<"Throughputs: ";
  for(unsigned int i=0; i<sesthr.size(); i++) outputFile<<doubleToString(sesthr[i])<<" ";
  outputFile<<std::endl;
  outputFile<<"Utility: ";
  for(unsigned int i=0; i<seslog.size(); i++) outputFile<<doubleToString(seslog[i])<<" ";
  outputFile<<"\n\n";

  return ;
}
void UdpSend(Ptr<Socket> sock, Ptr<Packet> packet, int sess, int path)
{
  myTag tag;
  tag.SetValue1(Simulator::Now().GetSeconds());
  tag.SetValue2((uint16_t)sess);
  tag.SetValue3(0);
  
  packet->AddPacketTag(tag); 
  sock->Send(packet);
}
void recvCallback(Ptr<Socket> socket)
{
  Address address;
  Ptr<Packet> packet=socket->RecvFrom(address);
  myTag tag;
  packet->PeekPacketTag(tag);
  double sendTime=tag.GetValue1();
  uint16_t iSession=tag.GetValue2();
  uint16_t qSize = tag.GetValue3();
  double recvTime=Simulator::Now().GetSeconds();
  double pacSize=packet->GetSize();
  socket->thrs.push_back(pacSize);
  socket->dels.push_back(recvTime-sendTime);
  socket->qSizes.push_back(qSize);
  Address sendaddr;
  Address recvaddr;
  socket->GetPeerName(sendaddr);
  socket->GetSockName(recvaddr);
  NS_LOG_LOGIC("recv callback, time:"<<recvTime<<", sendtime:"<<sendTime<<",packetsize:"<<pacSize<<", sender session:"<<iSession<<"qSize:"<<qSize);
}


/*set value functions*/
void DrlRouting::setErrp(double err)
{
  err_p=err;
}
void DrlRouting::setECNqThr(uint16_t thr)
{
  ECNqThr=thr;
}
void DrlRouting::setServerPort(int port)
{
  server_port=port;
}
void DrlRouting::setInputPath(string path)
{
  inputPath=path;
}
void DrlRouting::setOutputPath(string path)
{
  outputPath=path;
}
void DrlRouting::setUpTime(int time)
{
  upTime=time;
}
void DrlRouting::setStopTime(int time)
{
  stopTime=time;
}
void DrlRouting::setPacketSize(int size)
{
  packetSize=size;
}
void DrlRouting::setCap(string rate) //done
{
  cap=DataRate(rate.c_str());
}
void DrlRouting::setSendRate(vector<string> rates) // done
{
  for(unsigned int i=0; i<rates.size(); i++){
    DataRate rate=DataRate(rates[i].c_str());
    sendRates.push_back(rate);
  }
}
void DrlRouting::meanRatio()    //done
{
  int sn=sRatios.size();
  for(int i=0; i<sn; i++){
    int pn=sRatios[i].size();
    for(int j=0; j<pn; j++){
      sRatios[i][j]=1.0/(double)pn;
    }
  }
}

void DrlRouting::randRatio()
{
  int sn=sRatios.size();
  for(int i=0; i<sn; i++){
    int pn=sRatios[i].size();
    double sum=0;
    for(int j=0; j<pn; j++){
      sRatios[i][j]=getRand();
      sum+=sRatios[i][j];
    }
    for(int j=0; j<pn; j++) sRatios[i][j]=sRatios[i][j]/sum;
  }
}

