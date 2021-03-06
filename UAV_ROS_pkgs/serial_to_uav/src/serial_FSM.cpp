#include <SerialStream.h>
#include <iostream>
#include <unistd.h>
#include <signal.h>
#include "ros/ros.h"
#include "tf/transform_broadcaster.h"
#include "geometry_msgs/PoseStamped.h"
#include "geometry_msgs/Quaternion.h"
#include "geometry_msgs/Vector3.h"
#include "serial_to_uav/UAV.h"

//Yu Yun library
#include "math_basic.h"
#include "math_vector.h"
#include "math_matrix.h"
#include "math_quaternion.h"
#include "math_rotation.h"

using namespace std;
using namespace LibSerial ;

SerialStream serial_port ;
bool got_pose;
char next_byte1 = 'k', next_byte2 = 'k';
float out;
const int BUF_LEN = 44;     //11*4 -  q0 q1 q2 q3, a1, a2, a3, w1, w2, w3, h
const int OUT_LEN = BUF_LEN/4;
char float_array[BUF_LEN]; 
float out_array[OUT_LEN];
int error_count;
int connection_loss_count = 0;

bool is_write_on;
// is_write_on uav_ctrl info to serial port
int16_t yaw_rate = 1,pitch = 2,roll = 3;
int16_t vel = 4;

// read serial FSM
enum {
    IDLE1,
    IDLE2,
    LOADBUF,
    TAIL1,
    TAIL2,
    UNPACK,
};
int read_counter = 0;
uint8_t state = IDLE1;
int buffer_counter = 0;
int float_counter = 0;

ros::Subscriber sub;
ros::Publisher pub;

void serial_interrupt(int sig){ // can be called asynchronously
	if (serial_port.IsOpen())
		serial_port.Close();
	ros::shutdown();
}

void get_uav_ctrl(const geometry_msgs::Quaternion::ConstPtr& msg)
{
	int8_t tmpYaw[2]; 
	int8_t tmpPitch[2]; 
	int8_t tmpRoll[2]; 
	int8_t tmpVel[2]; 
	yaw_rate = (int16_t)msg->z;
	pitch = (int16_t)msg->x;
	roll = (int16_t)msg->y;
	vel = (int16_t)msg->w;	

    // this is an ugly solution to compensate IMU roll bias
    roll += 200;

	tmpYaw[1] = (int8_t)yaw_rate;
	tmpYaw[0] = (int8_t)(yaw_rate>>8);
	tmpPitch[1] = (int8_t)pitch;
	tmpPitch[0] = (int8_t)(pitch>>8);
	tmpRoll[1] = (int8_t)roll;
	tmpRoll[0] = (int8_t)(roll>>8);
	tmpVel[1] = (int8_t)vel;
	tmpVel[0] = (int8_t)(vel>>8);
	printf("yaw %i pitch %i roll %i\n", yaw_rate, pitch, roll-200);

	serial_port << "AA" 
				<< tmpYaw[0] << tmpYaw[1] 
				<< tmpPitch[0] << tmpPitch[1] 
				<< tmpRoll[0] << tmpRoll[1] 
				<< tmpVel[0] << tmpVel[1] 
				<<"BB";
} 

//void get_pose(const geometry_msgs::PoseStamped::ConstPtr& msg)
void get_pose()
{
	// generate a debug output, added on 04-11-2014
	float yaw, pitch, roll;
	vector4f q_debug;
    static tf::TransformBroadcaster br;
    tf::Transform tran;
	//x = msg->pose.position.x;
	//y = msg->pose.position.y;
	//yaw = asin(msg->pose.orientation.z)*2;
	got_pose = true;
	//if (serial_port.rdbuf()->in_avail() ==0 )
	//{
	//	connection_loss_count++;
	//}
    //ROS_INFO("Ready to get IMU data");
	serial_to_uav::UAV msg;
	while( serial_port.rdbuf()->in_avail() > 0  ) 
	{
		// if rdbuf is not empty, reset connection_loss_count
		connection_loss_count = 0;

        switch (state)
        {
            case IDLE1:
                    buffer_counter = 0;
		            serial_port.get(next_byte1);
		            if (next_byte1 == 'A')
                    {
                        state = IDLE2;
                    }
                    else
                    {
                        state = IDLE1;
                    }
                break;

            case IDLE2:
		            serial_port.get(next_byte1);
		            if (next_byte1 == 'A')
                    {
                        state = LOADBUF;
                        float_counter = 0;
                    }
                    else
                    {
                        state = IDLE1;
                    }
                break;

            case LOADBUF:
                    float_counter ++ ;
                    serial_port.get(float_array[buffer_counter*4+4-float_counter]);
                    read_counter ++;

                    if (float_counter == 4)
                    {
                        float_counter = 0;
                        buffer_counter++;
                        if (buffer_counter == OUT_LEN)
                        {
                            cout << "have read " << read_counter << " chars"<<endl;
                            for (int i=0; i< OUT_LEN;i++)
                            {
                                memcpy(&out,float_array+4*i,4);
                                printf("%4.3f\t",out);
                                out_array[i] = out;
                            }

                            printf("\n");
                            for (int k = 0; k < 4; k++)
                            {
                                if ((out_array[k] != out_array[k]) || 
                                    (out_array[k] >  999999) ||
                                    (out_array[k] < -999999))
                                {
                                    out_array[0] = 1; 
                                    out_array[1] = 0; 
                                    out_array[2] = 0; 
                                    out_array[3] = 0; 
                                }
                            }
                            for (int k = 4; k < 11; k++)
                            {
                                if ((out_array[k] != out_array[k]) || 
                                    (out_array[k] >  999999) ||
                                    (out_array[k] < -999999))
                                {
                                    out_array[k] = 0;
                                }
                            }

                            msg.orientation.w = out_array[0];
                            msg.orientation.x = out_array[1];
                            msg.orientation.y = out_array[2];
                            msg.orientation.z = out_array[3];
                            q_debug[0] = out_array[0];
                            q_debug[1] = out_array[1];
                            q_debug[2] = out_array[2];
                            q_debug[3] = out_array[3];
                            quat_to_eular(&yaw, &pitch, &roll, q_debug);
                            msg.debug.x = yaw;
                            msg.debug.y = pitch;
                            msg.debug.z = roll;
                            msg.linear_a.x = out_array[4] * 100;
                            msg.linear_a.y = out_array[5] * 100;
                            msg.linear_a.z = out_array[6] * 100;
                            msg.angular_v.x = out_array[7];
                            msg.angular_v.y = out_array[8];
                            msg.angular_v.z = out_array[9];
                            msg.height = out_array[10];
                            msg.header.stamp =ros::Time::now();
                            pub.publish(msg);
                            tran.setOrigin( tf::Vector3(0.0, 0.0, 0.0) );
                            tran.setRotation( tf::Quaternion(msg.orientation.x, 
                                                       msg.orientation.y, 
                                                       msg.orientation.z,
                                                       msg.orientation.w) );
                            br.sendTransform(tf::StampedTransform(tran, ros::Time::now(), "world","uav_frame"));
                            read_counter = 0;
                            state = IDLE1;
                        }
                    }
                break;

            case TAIL1:
		            serial_port.get(next_byte1);
                    cout << next_byte1 << endl;
		            if (next_byte1 == 'B')
                    {
                        state = TAIL2;
                    }
                    else
                    {
                        state = IDLE1;
                    }
                break;

            case TAIL2:
		            serial_port.get(next_byte1);
		            if (next_byte1 == 'B')
                    {
                        state = UNPACK;
                    }
                    else
                    {
                        state = IDLE1;
                    }
                break;

            case UNPACK:
                printf("unpack: ");
                for (int i=0; i< OUT_LEN;i++)
                {
                    memcpy(&out,float_array+4*i,4);
                    printf("%4.3f\t",out);
                    out_array[i] = out;
                }
                printf("\n ");

                break;
        } 


	} 
	if (connection_loss_count > 50)
		printf("connection loss!\n");
}

int main (int argc, char** argv)
{
	// Register signals 
  	signal(SIGINT, serial_interrupt); 
	serial_port.Open( "/dev/ttySAC2" ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "[" << __FILE__ << ":" << __LINE__ << "] "
			   << "Error: Could not open serial port."
			   << std::endl ;
	 exit(1) ;
	}
	//
	// Set the baud rate of the serial port.
	//
	serial_port.SetBaudRate( SerialStreamBuf::BAUD_115200 ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "Error: Could not set the baud rate." <<  
	std::endl ;
	 exit(1) ;
	}
	//
	// Set the number of data bits.
	//
	serial_port.SetCharSize( SerialStreamBuf::CHAR_SIZE_8 ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "Error: Could not set the character size." <<  
	std::endl ;
	 exit(1) ;
	}
	//
	// Disable parity.
	//
	serial_port.SetParity( SerialStreamBuf::PARITY_NONE ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "Error: Could not disable the parity." <<  
	std::endl ;
	 exit(1) ;
	}
	//
	// Set the number of stop bits.
	//
	serial_port.SetNumOfStopBits( 1 ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "Error: Could not set the number of stop bits."
			   << std::endl ;
	 exit(1) ;
	}
	//
	// Turn off hardware flow control.
	//
	serial_port.SetFlowControl( SerialStreamBuf::FLOW_CONTROL_NONE ) ;
	if ( ! serial_port.good() )
	{
	 std::cerr << "Error: Could not use hardware flow control."
			   << std::endl ;
	 exit(1) ;
	}	
	ros::init(argc, argv, "serial_talker");
	ros::NodeHandle n;
	//control read only or read & is_write_on
	n.param<bool>("is_write_on", is_write_on, false);
	//sub = n.subscribe("/slam_out_pose", 1, get_pose);
	pub = n.advertise<serial_to_uav::UAV>("uav_imu",100);
	
	// if read & is_write_on, listen to uav_control topic
	if (is_write_on)
		sub=n.subscribe("/board_ctrl", 1000, get_uav_ctrl);
		//sub=n.subscribe("/uav_ctrl", 1000, get_uav_ctrl);
	ros::Rate loop_rate(50);
	while (ros::ok())
	{
		get_pose();
		ros::spinOnce();
		loop_rate.sleep();
	}
	//ros::spin();
	return 0;
}
