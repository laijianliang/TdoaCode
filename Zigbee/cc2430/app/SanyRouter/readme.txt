该工程是三一项目第二阶段的跳传方案
总共包括三类设备，可能workspace中选择工程

1、采集数据端， 工程名CollectNode，射频采用2430
2、数据汇报端（即最后一跳发送给基站的一端），工程名ReportNode, 射频采用2430
3、跳传结点，工程名RouterNode, 射频采用2530
其中2430目录下的1、2是有用的，3是无用的。在2530的目录下1、2是无用的，3是有用的。

注意：正式产品化时请把无用的工程删除或另取别名以示区别。
