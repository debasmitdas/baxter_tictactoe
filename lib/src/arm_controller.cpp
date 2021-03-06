#include "arm_controller/arm_controller.h"

using namespace baxter_core_msgs;
using namespace geometry_msgs;
using namespace sensor_msgs;
using namespace std;
using namespace ttt;

/*
    TASKS:
    Make arm move at same time
    I would make  ArmController::hoverAboveTokens() return bool if success/failure. 
    And also you should check for the limb, because if by mistake you call the function 
    on the right limb you should check for that in the beginning and return false. 
*/

/******************* Public ************************/

int pose_delay = 0;

ArmController::ArmController(string limb): img_trp(n), limb(limb)
{ 
    if(limb != "left" && limb != "right"){
        ROS_ERROR("[Arm Controller] Invalid limb. Acceptable values are: right / left");
        ros::shutdown();
    }

    ROS_DEBUG_STREAM(cout << "[Arm Controller] " << limb << endl);

    // see header file for descriptions
    joint_cmd_pub = n.advertise<baxter_core_msgs::JointCommand>("/robot/limb/" + limb + "/joint_command", 1);   
    endpt_sub = n.subscribe("/robot/limb/" + limb + "/endpoint_state", 1, &ArmController::endpointCallback, this);
    img_sub = img_trp.subscribe("/cameras/left_hand_camera/image", 1, &ArmController::imageCallback, this);

    ir_sub = n.subscribe("/robot/range/left_hand_range/state", 1, &ArmController::IRCallback, this);

    ik_client = n.serviceClient<SolvePositionIK>("/ExternalTools/" + limb + "/PositionKinematicsNode/IKService");
    gripper = new Vacuum_Gripper(ttt::left);

    cv::namedWindow("[Arm Controller] hand camera", cv::WINDOW_NORMAL);

    NUM_JOINTS = 7;
    CENTER_X = 0.655298787334;
    CENTER_Y = 0.205732369738; 

    CELL_SIDE = 0.15;
    IR_RANGE_THRESHOLD = 0.085;
    curr_range = 0;
    curr_max_range = 0;
    curr_min_range = 0;
}

ArmController::~ArmController() {
    cv::destroyWindow("[Arm Controller] hand camera");
}

void ArmController::endpointCallback(const EndpointState& msg)
{
    // update current pose
    curr_pose = msg.pose;
}

void ArmController::imageCallback(const ImageConstPtr& msg)
{
    cv_bridge::CvImageConstPtr cv_ptr;
    try
    {
        cv_ptr = cv_bridge::toCvShare(msg);
    }
    catch(cv_bridge::Exception& e)
    {
        ROS_ERROR("[Arm Controller] cv_bridge exception: %s", e.what());
    }

    cv::imshow("[Arm Controller] hand camera", cv_ptr->image.clone());
    cv::waitKey(30);
}

void ArmController::IRCallback(const RangeConstPtr& msg)
{
    ROS_DEBUG_STREAM(cout << "range: " << msg->range << " max range: " << msg->max_range << " min range: " << msg->min_range << endl);
    ROS_DEBUG_STREAM(cout << "range: " << curr_range << " max range: " << curr_max_range << " min range: " << curr_min_range << endl);

    // update current range
    curr_range = msg->range;
    curr_max_range = msg->max_range;
    curr_min_range = msg->min_range;
}

void ArmController::pickUpToken()
{
    if(limb == "right") 
    {
        ROS_ERROR("[Arm Controller] Right arm should not pick up tokens");
    }
    else if(limb == "left")
    {
        bool no_token = true;
        while(no_token)
        {
            hoverAboveTokens(STRICTPOSE);
            // grip token; record if arm fails successfully gripped token
            if(gripToken()) no_token = false;
            // if(gripToken()) no_token = false;
            hoverAboveTokens(LOOSEPOSE);   
            // check if arm successfully gripped token
            // (sometimes infrared sensor falls below threshold w/o 
            // successfully gripping token)
            if(!(gripper->is_gripping()))
            {
                no_token = true;
                // gripper cannot suck w/o blowing first
                gripper->blow();   
            } 
        }    
    }
}

void ArmController::placeToken(int cell_num)
{
    if(limb == "right") 
    {
        ROS_ERROR("[Arm Controller] Right arm should not pick up tokens");
    }
    else if(limb == "left")
    {
        hoverAboveBoard();
        releaseToken(cell_num);
        hoverAboveBoard();
        hoverAboveTokens(STRICTPOSE);
    }
}

void ArmController::moveToRest() 
{
    vector<float> joint_angles;
    joint_angles.resize(NUM_JOINTS);

    // requested position and orientation is filled out despite hardcoding of joint angles
    // to double-check if move has been completed using hasPoseCompleted()
    req_pose_stamped.header.frame_id = "base";
    req_pose_stamped.pose.position.x = 0.292391;
    req_pose_stamped.pose.position.z = 0.181133;
    req_pose_stamped.pose.orientation.x = 0.028927;
    req_pose_stamped.pose.orientation.y = 0.686745;
    req_pose_stamped.pose.orientation.z = 0.00352694;
    req_pose_stamped.pose.orientation.w = 0.726314;

    // joint angles are hardcoded as opposed to relying on the IK solver to provide a solution given
    // the requested pose because testing showed that the IK solver would fail approx. 9/10 to find 
    // a joint angles combination for the left arm rest pose.
    if(limb == "left")
    {
        joint_angles[0] = 1.1508690861110316;
        joint_angles[1] = -0.6001699832601681;
        joint_angles[2] = -0.17449031462196582;
        joint_angles[3] = 2.2856313739492666;
        joint_angles[4] = 1.8680051044474626;
        joint_angles[5] = -1.4684031092033123;
        joint_angles[6] = 0.1257864246066039;
        req_pose_stamped.pose.position.y = 0.611039; 
    }
    else if(limb == "right") 
    {
        joint_angles[0] = -1.3322623142784817;
        joint_angles[1] = -0.5786942522297723;
        joint_angles[2] = 0.14266021327334347;
        joint_angles[3] = 2.2695245756764697;
        joint_angles[4] = -1.9945585194480093;
        joint_angles[5] = -1.469170099597255;
        joint_angles[6] = -0.011504855909140603;
        req_pose_stamped.pose.position.y = -0.611039; 
    }

    publishMoveCommand(joint_angles, LOOSEPOSE);
}

/******************* Private ************************/

void ArmController::hoverAboveTokens(GoalType goal)
{
    req_pose_stamped.header.frame_id = "base";
    req_pose_stamped.pose.position.x = 0.540298787334;
    req_pose_stamped.pose.position.y = 0.683732369738;
    req_pose_stamped.pose.position.z = 0.15621169853;

    req_pose_stamped.pose.orientation.x = 0.712801568376;
    req_pose_stamped.pose.orientation.y = -0.700942136419;
    req_pose_stamped.pose.orientation.z = -0.0127158080742;
    // req_pose_stamped.pose.orientation.z = -0.0127158080742;
    req_pose_stamped.pose.orientation.w = -0.0207931175453;
    // req_pose_stamped.pose.orientation.w = -0.0207931175453;

    vector<float> joint_angles = getJointAngles(req_pose_stamped);
    publishMoveCommand(joint_angles, goal);
}

bool ArmController::gripToken()
{
    req_pose_stamped.header.frame_id = "base";
    req_pose_stamped.pose.position.x = 0.540298787334;
    req_pose_stamped.pose.position.y = 0.683732369738;
    req_pose_stamped.pose.position.z = -0.10501169853;

    req_pose_stamped.pose.orientation.x = 0.712801568376;
    req_pose_stamped.pose.orientation.y = -0.700942136419;
    req_pose_stamped.pose.orientation.z = -0.0127158080742;
    req_pose_stamped.pose.orientation.w = -0.0207931175453;

    vector<float> joint_angles = getJointAngles(req_pose_stamped);

    if(publishMoveCommand(joint_angles, COLLISION) == true) return true;
    else return false;
}

void ArmController::hoverAboveBoard()
{
    req_pose_stamped.header.frame_id = "base";
    req_pose_stamped.pose.position.x = CENTER_X;
    req_pose_stamped.pose.position.y = CENTER_Y;
    req_pose_stamped.pose.position.z = 0.13621169853;

    req_pose_stamped.pose.orientation.x = 0.712801568376;
    req_pose_stamped.pose.orientation.y = -0.700942136419;
    req_pose_stamped.pose.orientation.z = -0.0127158080742;
    req_pose_stamped.pose.orientation.w = -0.0207931175453;

    vector<float> joint_angles = getJointAngles(req_pose_stamped);
    publishMoveCommand(joint_angles, LOOSEPOSE);   
}

void ArmController::releaseToken(int cell_num)
{
    if((cell_num - 1) / 3 == 0) req_pose_stamped.pose.position.x = CENTER_X + CELL_SIDE;
    if((cell_num - 1) / 3 == 1) req_pose_stamped.pose.position.x = CENTER_X;
    if((cell_num - 1) / 3 == 2) req_pose_stamped.pose.position.x = CENTER_X - CELL_SIDE;
    if(cell_num % 3 == 0) req_pose_stamped.pose.position.y = CENTER_Y - CELL_SIDE;
    if(cell_num % 3 == 1) req_pose_stamped.pose.position.y = CENTER_Y + CELL_SIDE;
    if(cell_num % 3 == 2) req_pose_stamped.pose.position.y = CENTER_Y;

    req_pose_stamped.pose.position.z = -0.13501169853;

    req_pose_stamped.pose.orientation.x = 0.712801568376;
    req_pose_stamped.pose.orientation.y = -0.700942136419;
    req_pose_stamped.pose.orientation.z = -0.0127158080742;
    req_pose_stamped.pose.orientation.w = -0.0207931175453;

    vector<float> joint_angles = getJointAngles(req_pose_stamped);
    publishMoveCommand(joint_angles, STRICTPOSE);   
    gripper->blow();
}

vector<float> ArmController::getJointAngles(PoseStamped pose_stamped)
{
    // IK solver service
    SolvePositionIK ik_srv;

    ik_srv.request.pose_stamp.push_back(pose_stamped);
    ik_client.call(ik_srv);

    ROS_DEBUG_STREAM(cout << "[Arm Controller] " << ik_srv.request << endl);
    ROS_DEBUG_STREAM(cout << "[Arm Controller] " << ik_srv.response << endl);   
    
    // if service is successfully called
    if(ik_client.call(ik_srv))
    {
        ROS_DEBUG("[Arm Controller] Service called");

        // store joint angles values received from service
        vector<float> joint_angles;
        joint_angles.resize(NUM_JOINTS);
        for(int i = 0; i < ik_srv.response.joints[0].position.size(); i++)
        {
            joint_angles[i] = ik_srv.response.joints[0].position[i];
        }

        bool all_zeros = true;
        for(int i = 0; i < joint_angles.size(); i++){
            if(joint_angles[i] == 0) all_zeros = false;
            ROS_DEBUG("[Arm Controller] Joint angles %d: %0.4f", i, joint_angles[i]);
        }
        if(all_zeros == false) ROS_ERROR("[Arm Controller] Angles are all 0 radians (No solution found)");

        ros::Duration(2).sleep(); 
        return joint_angles;
    }
    else 
    {
        ROS_ERROR("[Arm Controller] SolvePositionIK service was unsuccessful");
        vector<float> empty;
        return empty;
    }
}

bool ArmController::publishMoveCommand(vector<float> joint_angles, GoalType goal) 
{
    ros::Rate loop_rate(100);
    JointCommand joint_cmd;

    // command in position mode
    joint_cmd.mode = JointCommand::POSITION_MODE;

    // command joints in the order shown in baxter_interface
    joint_cmd.names.push_back(limb + "_s0");
    joint_cmd.names.push_back(limb + "_s1");
    joint_cmd.names.push_back(limb + "_e0");
    joint_cmd.names.push_back(limb + "_e1");
    joint_cmd.names.push_back(limb + "_w0");
    joint_cmd.names.push_back(limb + "_w1");
    joint_cmd.names.push_back(limb + "_w2");

    // set your calculated velocities
    joint_cmd.command.resize(NUM_JOINTS);

    // set joint angles
    for(int i = 0; i < NUM_JOINTS; i++)
    {
        joint_cmd.command[i] = joint_angles[i];
    }

    // record time at which joint angles was published to arm
    ros::Time start_time = ros::Time::now();

    while(ros::ok())
    {
        // ROS_DEBUG("[Arm Controller] In the loop");
        joint_cmd_pub.publish(joint_cmd);
        ros::spinOnce();
        loop_rate.sleep();


        if(goal == STRICTPOSE)
        {
            if(hasPoseCompleted(STRICT)) 
            {
                ROS_DEBUG("[Arm Controller] Move completed");
                return true;
            }     
            else {
                // if 10 seconds has elapsed and pose has not been achieved,
                // (pose was likely unreachable) stop publishing joint angles
                ros::Time curr_time = ros::Time::now();
                if((curr_time - start_time).toSec() > 8)
                {
                    ROS_ERROR("8 seconds have elapsed. No collision has occured.");
                    return false;
                } 
                ROS_DEBUG("No collision yet");
            }         
        }
        else if(goal == LOOSEPOSE)
        {
            if(hasPoseCompleted(LOOSE)) 
            {
                ROS_INFO("pose delay %d", pose_delay);
                pose_delay = 0;
                ROS_DEBUG("[Arm Controller] Move completed");
                return true;
            }     
            else {
                // if 10 seconds has elapsed and pose has not been achieved,
                // (pose was likely unreachable) stop publishing joint angles
                ros::Time curr_time = ros::Time::now();
                if((curr_time - start_time).toSec() > 8)
                {
                    ROS_ERROR("8 seconds have elapsed. No collision has occured.");
                    return false;
                } 
                ROS_DEBUG("No collision yet");
            }                  
        }
        else if(goal == COLLISION)
        {
            if(hasCollided())
            {
                gripper->suck();
                return true;
            }
            else {
                // if 10 seconds has elapsed and pose has not been achieved,
                // (probably no object to collide with) stop publishing joint angles
                ros::Time curr_time = ros::Time::now();
                if((curr_time - start_time).toSec() > 10)
                {
                    ROS_ERROR("10 seconds have elapsed. No collision has occured.");
                    return false;
                } 
                ROS_DEBUG("No collision yet");
            }
        }
    }   
}

bool ArmController::hasPoseCompleted(PoseType pose)
{
    bool same_pose = true;

    ROS_INFO("Checking if pose has been completed. Strategy: %s",pose==STRICT?"strict":"loose");

    if(pose == STRICT)
    {
        if(!withinTwoHundreth(curr_pose.position.x, req_pose_stamped.pose.position.x)) same_pose = false; 
        if(!withinTwoHundreth(curr_pose.position.y, req_pose_stamped.pose.position.y)) same_pose = false;
        if(!withinTwoHundreth(curr_pose.position.z, req_pose_stamped.pose.position.z)) same_pose = false;        
    }
    else if(pose == LOOSE)
    {
        if(!withinFourHundreth(curr_pose.position.x, req_pose_stamped.pose.position.x)) same_pose = false;  
        if(!withinFourHundreth(curr_pose.position.y, req_pose_stamped.pose.position.y)) same_pose = false;  
        if(!withinFourHundreth(curr_pose.position.z, req_pose_stamped.pose.position.z)) same_pose = false;              
    }

    if (same_pose == true)
    {
        pose_delay++;
        ROS_INFO("pose delay: %d", pose_delay);
        ROS_INFO("Position is ok!");
        ROS_INFO_STREAM(cout << curr_pose << endl);
        ROS_INFO_STREAM(cout << req_pose_stamped.pose << endl);
    }

    if(pose == STRICT)
    {
        if(!equalTwoDP(curr_pose.orientation.x, req_pose_stamped.pose.orientation.x)) same_pose = false;
        if(!equalTwoDP(curr_pose.orientation.y, req_pose_stamped.pose.orientation.y)) same_pose = false;
        if(!equalTwoDP(curr_pose.orientation.z, req_pose_stamped.pose.orientation.z)) same_pose = false;
        if(!equalTwoDP(curr_pose.orientation.w, req_pose_stamped.pose.orientation.w)) same_pose = false;
    }
    else if(pose == LOOSE)
    {
        if(!withinFourHundreth(curr_pose.orientation.x, req_pose_stamped.pose.orientation.x)) same_pose = false;
        if(!withinFourHundreth(curr_pose.orientation.y, req_pose_stamped.pose.orientation.y)) same_pose = false;
        if(!withinFourHundreth(curr_pose.orientation.z, req_pose_stamped.pose.orientation.z)) same_pose = false;
        if(!withinFourHundreth(curr_pose.orientation.w, req_pose_stamped.pose.orientation.w)) same_pose = false;   
    }


    if (same_pose == true) ROS_INFO("Position and orientation are ok!");

    return same_pose;    
}

bool ArmController::hasCollided()
{
    ROS_DEBUG_STREAM(cout << " range: " << curr_range << " max range: " << curr_max_range << " min range: " << curr_min_range << endl);
    if(curr_range <= curr_max_range && curr_range >= curr_min_range && curr_range <= IR_RANGE_THRESHOLD)
    {
        ROS_INFO("[Arm Controller] Collision");
        return true;
    }
    else {
        return false;
    }
}

bool ArmController::withinTwoHundreth(float x, float y)
{
    float diff = abs(x - y);
    float diffTwoDP = roundf(diff * 100) / 100;
    return diffTwoDP <= 0.02 ? true : false;    
}

bool ArmController::withinThreeHundreth(float x, float y)
{
    float diff = abs(x - y);
    float diffTwoDP = roundf(diff * 100) / 100;
    return diffTwoDP <= 0.03 ? true : false;    
}

bool ArmController::withinFourHundreth(float x, float y)
{
    float diff = abs(x - y);
    float diffTwoDP = roundf(diff * 100) / 100;
    return diffTwoDP <= 0.04 ? true : false;    
}

bool ArmController::withinEightHundreth(float x, float y)
{
    float diff = abs(x - y);
    float diffTwoDP = roundf(diff * 100) / 100;
    return diffTwoDP <= 0.08 ? true : false;    
}

bool ArmController::equalTwoDP(float x, float y) 
{
    float xTwoDP = roundf(x * 100) / 100;
    float yTwoDP = roundf(y * 100) / 100;
    return xTwoDP == yTwoDP ? true : false;
}

bool ArmController::equalOneDP(float x, float y) 
{
    float xTwoDP = roundf(x * 10) / 10;
    float yTwoDP = roundf(y * 10) / 10;
    return xTwoDP == yTwoDP ? true : false;
}