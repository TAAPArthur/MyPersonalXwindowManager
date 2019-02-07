#include "../UnitTests.h"
#include "../TestX11Helper.h"
#include "../../monitors.h"
#include "../../globals.h"

START_TEST(test_detect_monitors){
    detectMonitors();
    assert(isNotEmpty(getAllMonitors()));
    LOG(LOG_LEVEL_NONE,"Detected %d monitor\n",getSize(getAllMonitors()));
}END_TEST
START_TEST(test_dock_add_remove){
    int id=1;
    WindowInfo*winInfo=createWindowInfo(id);
    markAsDock(winInfo);
    assert(addWindowInfo(winInfo));
    assert(!addWindowInfo(winInfo));
    assert(removeWindow(id));
    assert(!removeWindow(id));
}END_TEST
START_TEST(test_avoid_struct){
    int dim=100;
    int dockSize=10;
    updateMonitor(2, 0, (short[]){dim, 0, dim,dim});
    updateMonitor(1, 1, (short[]){0, 0, dim,dim});

    int arrSize=_i==0?12:4;
    Monitor* monitor=getValue(getAllMonitors());
    Monitor* sideMonitor=getValue(getAllMonitors()->next);
    assert(monitor!=NULL);
    assert(sideMonitor!=NULL);
    assert(sideMonitor->id==2);
    assert(monitor->id==1);

    short arr[4][4]={
            {0,0,dockSize,monitor->height},//left
            {monitor->width-dockSize,0,dockSize,monitor->height},
            {0,0,monitor->width,dockSize},//top
            {0,monitor->height-dockSize,monitor->width,dockSize}
    };

    for(int i=0;i<4;i++){
        WindowInfo*info=createWindowInfo(i+1);
        markAsDock(info);
        info->onlyOnPrimary=1;
        int properties[12]={0};

        properties[i]=dockSize;
        properties[i*2+4]=0;
        properties[i*2+4+1]=dim;

        setDockArea(info, arrSize, properties);

        assert(addWindowInfo(info));
        assert(intersects(arr[i], &monitor->x));
        assert(intersects(arr[i], &monitor->viewX)==0);

        assert(!intersects(arr[i], &sideMonitor->x));
        for(int n=0;n<4;n++)
            assert((&sideMonitor->x)[n]==(&sideMonitor->viewX)[n]);


        removeWindow(info->id);
        assert(intersects(arr[i], &monitor->viewX));
    }
    for(int c=0;c<3;c++){
        WindowInfo*info=createWindowInfo(c+1);
        info->onlyOnPrimary=1;
        int properties[12];
        for(int i=0;i<4;i++){
            properties[i]=dockSize-c/2;
            properties[i*2+4]=0;
            properties[i*2+4+1]=dim;
        }
        markAsDock(info);
        setDockArea(info, arrSize, properties);
        assert(addWindowInfo(info));
    }
    assert(monitor->viewWidth*monitor->viewHeight == (dim-dockSize*2)*(dim-dockSize*2));
    assert(sideMonitor->viewWidth*sideMonitor->viewHeight == dim*dim);

}END_TEST

START_TEST(test_intersection){
    int dimX=100;
    int dimY=200;
    int offsetX=10;
    int offsetY=20;
    short rect[]={offsetX, offsetY, dimX, dimY};

#define _intersects(arr,x,y,w,h) intersects(arr,(short int *)(short int[]){x,y,w,h})
    //easy to see if fail
    assert(!_intersects(rect, 0,0,offsetX,offsetY));
    assert(!_intersects(rect, 0,offsetY,offsetX,offsetY));
    assert(!_intersects(rect, offsetX,0,offsetX,offsetY));
    for(int x=0;x<dimX+offsetX*2;x++){
        assert(_intersects(rect, x, 0, offsetX, offsetY)==0);
        assert(_intersects(rect, x, offsetY+dimY, offsetX, offsetY)==0);
    }
    for(int y=0;y<dimY+offsetY*2;y++){
        assert(_intersects(rect, 0, y, offsetX, offsetY)==0);
        assert(_intersects(rect, offsetX+dimX,y, offsetX, offsetY)==0);
    }
    assert(_intersects(rect, 0,offsetY, offsetX+1, offsetY));
    assert(_intersects(rect, offsetX+1,offsetY, 0, offsetY));
    assert(_intersects(rect, offsetX+1,offsetY, dimX, offsetY));

    assert(_intersects(rect, offsetX,0, offsetX, offsetY+1));
    assert(_intersects(rect, offsetX,offsetY+1, offsetX, 0));
    assert(_intersects(rect, offsetX,offsetY+1, offsetX, dimY));

    assert(_intersects(rect, offsetX+dimX/2,offsetY+dimY/2, 0, 0));

}END_TEST
START_TEST(test_fake_dock){
    WindowInfo*winInfo=createWindowInfo(1);
    markAsDock(winInfo);
    loadDockProperties(winInfo);
    addWindowInfo(winInfo);
    for(int i=0;i<LEN(winInfo->properties);i++)
        assert(winInfo->properties[i]==0);
}END_TEST
START_TEST(test_avoid_docks){
    int size=10;
    for(int i=3;i>=0;i--){
        WindowInfo*winInfo=createWindowInfo(createDock(i,size,_i));
        loadDockProperties(winInfo);
        markAsDock(winInfo);
        addWindowInfo(winInfo);
    }
    assert(getSize(getAllDocks())==4);
    Monitor*m=getValue(getAllMonitors());

    short arr[4][4]={
            {0,0,size,m->height},//left
            {m->width-size,0,size,m->height},
            {0,0,m->width,size},//top
            {0,m->height-size,m->width,size}
    };
    Node*n=getAllDocks();
    int index=0;
    FOR_EACH(n,
        WindowInfo*winInfo=getValue(n);

        int type=index++;
        assert(winInfo->properties[type]);
        for(int j=0;j<4;j++)
            if(j!=type)
                assert(winInfo->properties[j]==0);

    );

    for(int i=0;i<4;i++)
        assert(intersects(&m->viewX,arr[i])==0);

}END_TEST

START_TEST(test_root_dim){
    int width=0,height=0;
    Node*n=getAllMonitors();
    FOR_EACH(n,
        Monitor*m=getValue(n);
        width+=m->width;
        height+=m->height;
    )
    assert(width==getRootWidth());
    assert(height==getRootHeight());
}END_TEST





START_TEST(test_monitor_init){

    int size=5;
    int count=0;
    addFakeMaster(1,1);
    for(int x=0;x<2;x++)
        for(int y=0;y<2;y++){
            short dim[4]={x,y,size*size+y,size*2+x};
            assert(updateMonitor(count, !count, dim));
            Monitor *m=getValue(getAllMonitors());
            assert(m->id==count);
            assert(isPrimary(m)==!count);
            for(int i=0;i<4;i++){
                assert((&m->x)[i]==(&m->viewX)[i]);
                assert(dim[i]==(&m->x)[i]);
            }
            assert(getWorkspaceFromMonitor(m));
            assert(getWorkspaceFromMonitor(m)->id==count++);
        }
}END_TEST
START_TEST(test_monitor_add_remove){

    addFakeMaster(1,1);
    int size=getNumberOfWorkspaces();
    for(int n=0;n<2;n++){
        for(int i=1;i<=size+1;i++){
            updateMonitor(i, 1, (short[]){0, 0, 100, 100});
            Workspace *w=getWorkspaceFromMonitor(getValue(isInList(getAllMonitors(), i)));
            if(i>size)
                assert(!w);
            else assert(w);
        }
        assert(getSize(getAllMonitors())==size+1);
    }
    //for(int i=0;i<getNumberOfWorkspaces();i++)
        //assert(getWorkspaceByIndex(i)->monitor==NULL);
    while(isNotEmpty(getAllMonitors()))
        assert(removeMonitor(getIntValue(getAllMonitors())));
    for(int i=0;i<getNumberOfWorkspaces();i++)
        assert(getWorkspaceByIndex(i)->monitor==NULL);
    assert(!removeMonitor(0));
}END_TEST









Suite *monitorsSuite() {
    Suite*s = suite_create("Monitors");
    TCase* tc_core;
    
    tc_core = tcase_create("Monitor");
    tcase_add_checked_fixture(tc_core, createSimpleContext, destroyContext);
    tcase_add_test(tc_core, test_monitor_init);
    tcase_add_test(tc_core,test_monitor_add_remove);
    suite_add_tcase(s, tc_core);

    tc_core = tcase_create("Detect monitors");
    tcase_add_checked_fixture(tc_core, createContextAndSimpleConnection, destroyContextAndConnection);
    tcase_add_test(tc_core, test_detect_monitors);
    tcase_add_test(tc_core, test_root_dim);
    suite_add_tcase(s, tc_core);

    tc_core = tcase_create("Docks");
    tcase_add_checked_fixture(tc_core, createContextAndSimpleConnection, destroyContextAndConnection);
    tcase_add_test(tc_core, test_intersection);
    tcase_add_loop_test(tc_core, test_avoid_struct,0,2);
    tcase_add_loop_test(tc_core, test_avoid_docks,0,2);
    tcase_add_test(tc_core,test_fake_dock);
    tcase_add_test(tc_core,test_dock_add_remove);




    suite_add_tcase(s, tc_core);
    return s;
}
