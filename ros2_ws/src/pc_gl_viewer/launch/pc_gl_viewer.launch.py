"""一键起「合成雷达 + OpenGL 查看器」。

用法：
    ros2 launch pc_gl_viewer pc_gl_viewer.launch.py
    ros2 launch pc_gl_viewer pc_gl_viewer.launch.py voxel:=0.15 color_mode:=1
    ros2 launch pc_gl_viewer pc_gl_viewer.launch.py rate_hz:=5.0 rings:=64

注意：两个节点都在前台跑，查看器需要图形界面（WSL2 需要 WSLg；纯 ssh 无 X 转发会报
glfwCreateWindow 失败）。
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    rate_hz = LaunchConfiguration('rate_hz')
    rings = LaunchConfiguration('rings')
    az_res = LaunchConfiguration('az_resolution_deg')
    voxel = LaunchConfiguration('voxel')
    color_mode = LaunchConfiguration('color_mode')
    point_size = LaunchConfiguration('point_size')
    topic = LaunchConfiguration('topic')

    args = [
        DeclareLaunchArgument('rate_hz', default_value='10.0', description='发布频率 Hz'),
        DeclareLaunchArgument('rings', default_value='32', description='雷达线数'),
        DeclareLaunchArgument('az_resolution_deg', default_value='0.25', description='水平角分辨率'),
        DeclareLaunchArgument('topic', default_value='points', description='点云话题名'),
        DeclareLaunchArgument('voxel', default_value='0.0', description='体素降采样边长(米)，0=关闭'),
        DeclareLaunchArgument('color_mode', default_value='0', description='着色: 0=强度 1=高度 2=距离'),
        DeclareLaunchArgument('point_size', default_value='2.4', description='点像素基准大小'),
    ]

    publisher = Node(
        package='pc_gl_viewer',
        executable='pc_cloud_publisher',
        name='pc_cloud_publisher',
        output='screen',
        parameters=[{
            'rate_hz': ParameterValue(rate_hz, value_type=float),
            'rings': ParameterValue(rings, value_type=int),
            'az_resolution_deg': ParameterValue(az_res, value_type=float),
        }],
        remappings=[('points', topic)],
    )

    viewer = Node(
        package='pc_gl_viewer',
        executable='pc_gl_viewer',
        name='pc_gl_viewer',
        output='screen',
        parameters=[{
            'voxel_leaf': ParameterValue(voxel, value_type=float),
            'color_mode': ParameterValue(color_mode, value_type=int),
            'point_size': ParameterValue(point_size, value_type=float),
            'topic': ParameterValue(topic, value_type=str),
        }],
    )

    return LaunchDescription(args + [publisher, viewer])
