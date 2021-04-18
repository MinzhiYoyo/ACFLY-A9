#ifndef ACFLY_COMMAND__HPP
#define ACFLY_COMMAND__HPP
#include "LMZLink.hpp"
#include "string"
#include "ControlSystem.hpp"
#include "TOF.hpp"
#include "raspberry.h"
//#include "common_ACFLY.h"
#include "NavCmdProcess.hpp"

using namespace std;

class ACFLYcmd{

private:
				int sum ;
	
public:
	ACFLYcmd(){
		LMZ_TOF::TOF_setBauds(921600);
		freeStatus = true ;
		pos_judge = 0 ;
		Curcmd.clearAllData();
		sum = 0 ;
		this->freq = 50;
		init_NavCmdInf(&(this->navInf));
	}
//		ACFLYcmd(NavCmdInf t){
//			this->navInf = t;
//		LMZ_TOF::TOF_setBauds(921600);
//		freeStatus = true ;
//		pos_judge = 0 ;
//		Curcmd.clearAllData();
//		sum = 0 ;
//		this->freq = 50;
//	}
	
	// 属性
	bool freeStatus;
	LMZLink Curcmd;
	LMZ_TOF myTOF ;
	int pos_judge ;
	double freq ;
	NavCmdInf navInf;
	
	// 方法
	bool JudgeMode(string &strcmd){
		if(strcmd.length() != 0)
				this->Curcmd = LMZLink::data_analyze(strcmd);
		freeStatus = false ;
		switch(this->Curcmd.mode)
		{
			case LMZLink::MODE::speedControl:
					this->SpeedControl();
				break;
			
			case LMZLink::MODE::distanceControl:	
					this->DistanceControl();
				break;
			
			case LMZLink::MODE::takeoff:	
					this->Takeoff();
				break;
			
			case LMZLink::MODE::land:	
					this->LandMode();
				break;
			
			case LMZLink::MODE::Wall_Distance:	
					this->Distance_wall();
				break;
			
			case LMZLink::MODE::NoMode:
					return true ;
			
			default:
				return false;
		}
		return true ;
	}	
	
	// 速度控制
	void SpeedControl(){
		double vx,vy,vz;
		for( int i = 0; i < Curcmd.params.size(); i++)
		{
			if(Curcmd.params[i] == std::string("vx"))
				vx = Curcmd.paramsValue[i];
			if(Curcmd.params[i] == std::string("vy"))
				vy = Curcmd.paramsValue[i];
			if(Curcmd.params[i] == std::string("vz"))
				vz = Curcmd.paramsValue[i];
		}
			Position_Control_set_TargetVelocityBodyHeadingXY_AngleLimit(vx,vy);
			Position_Control_set_TargetVelocityZ(vz);
			freeStatus = true ;
	} // speedControl end
	
	
	//距离控制
	void DistanceControl(){
		double posx,posy,posz;
		for (int i = 0; i < Curcmd.params.size(); i++)
		{
			if (Curcmd.params[i] == std::string("posx"))
			{
				posx = Curcmd.paramsValue[i];  
			}	
			else if (Curcmd.params[i] == std::string("posy"))
			{
				posy = Curcmd.paramsValue[i];  
			} 
			else if (Curcmd.params[i] == std::string("posz"))
			{
				posz = Curcmd.paramsValue[i];  
			} 
		}
/*-------------------------执行命令------------------------------------------*/
			if(this->pos_judge == 0)
			{
				Position_Control_set_TargetPositionXYZRelativeBodyheading(posx,posy,posz);
				this->pos_judge = 1;
			}
			if(this->pos_judge == 1)
			{
				Position_ControlMode pos_mode;   
				get_Position_ControlMode(&pos_mode);
				if(pos_mode == Position_ControlMode_Position)      
				{
					this->pos_judge = 0;
					Curcmd.clearAllData();
					freeStatus = true ;
					LMZ_SendMessage(string("distanceControlOK"));
				}
			}
	}
	
	//起飞
	void Takeoff(){
		double alt = 0;
		for (int i = 0; i < Curcmd.params.size(); i++)
		{
			if (Curcmd.params[i] == std::string("alt"))
			{
				alt = Curcmd.paramsValue[i]; 					
			}  
		}
/*----------------------执行命令------------------------------------*/
		double params[7];
						params[0] = 0;
						params[3] = nan("");
						params[4] = 0;	params[5] = 0;
						params[6] = alt / 100;
		int16_t res = Process_NavCmd( MAV_CMD_NAV_TAKEOFF, freq, MAV_FRAME_BODY_FLU, params, &navInf );
		if( NavCmdRs_SuccessOrFault(res) )
		{	//起飞完成
					init_NavCmdInf(&navInf);  // 重新初始化
					Curcmd.clearAllData();
					freeStatus = true ;
					LMZ_SendMessage(string("takeoffOK"));
		}
	}//起飞结束
  
	//控制离墙距离
	void Distance_wall(){
		double pidvx,maxvx;

		double distance_wall,wall_dis = 0;
		for (int i = 0; i < Curcmd.params.size(); i++)
		{
			if (Curcmd.params[i] == std::string("wall_dis"))
			{
				wall_dis = Curcmd.paramsValue[i];  
			}  
		}
/*---------------------执行命令------------------------*/
		distance_wall = myTOF.getTOFdistance();  //获取距离
//									HHK_SendDistance(distance_wall);
		pidvx = distance_wall - wall_dis; //获取差值
		
		if(abs(pidvx) <= 5)         //贴入精确范围后缩小幅度
		{
			maxvx = 3;
			this->sum++;
		}
		else
		{
			maxvx = 5;
			this->sum = 0;
		}
		if(abs(pidvx) > maxvx)
		{	
			pidvx = maxvx*(pidvx/abs(pidvx));  //限幅
		}
//									HHK_SendDistance(pidvx); 
		LMZ_SendMessage("distance_wall = " + to_string(distance_wall) + " , pidvx = " + to_string(pidvx) + "sum = " + to_string(this->sum));
		if(distance_wall <  0)
			pidvx = 0;
		Position_Control_set_TargetVelocityBodyHeadingXY_AngleLimit(pidvx,0);
		Position_Control_set_ZLock();
		if(this->sum == 5)   //确保位置稳定
		{
			this->sum = 0;
			Curcmd.clearAllData();
			freeStatus = true ;
			Position_Control_set_XYLock();
			Position_Control_set_ZLock();
			LMZ_SendMessage(string("Wall_DistanceOK"));
		}
	
	}
	
	
	void LandMode(){
	for (int i = 0; i < Curcmd.params.size(); i++)
	{
		if (Curcmd.params[i] == std::string("land"))
		{
			Position_Control_set_XYLock();
			Position_Control_set_TargetVelocityZ(-50);
			LMZ_SendMessage(string("landOK"));
		}
	}
}
	
};
#endif

