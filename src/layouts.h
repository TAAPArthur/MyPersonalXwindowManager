/**
 * @file layouts.h
 *
 * @brief Layout functions
 */

#ifndef LAYOUTS_H_
#define LAYOUTS_H_

#include "mywm-structs.h"
#include "wmfunctions.h"
#include "masters.h"
#include "workspaces.h"

/// size of config parameters
#define CONFIG_LEN 6
enum {CONFIG_INDEX_X = 0, CONFIG_INDEX_Y = 1, CONFIG_INDEX_WIDTH, CONFIG_INDEX_HEIGHT, CONFIG_INDEX_BORDER, CONFIG_INDEX_STACK};

/// Number of transformations
#define TRANSFORM_LEN (4)

/**
 * Transformations that can be applied to layouts.
 * Everything is relative to the midpoint of the view port of the monitor
 */
typedef enum {
    /// Apply no transformation
    NONE = 0,
    /// Rotate by 180 deg
    ROT_180,
    /// Reflect across x axis
    REFLECT_HOR,
    /// Reflect across the y axis
    REFLECT_VERT
} Transform;
/// Represents a field(s) in Layout Args
typedef enum {
    LAYOUT_LIMIT = 0,
    /// represents all padding fields
    LAYOUT_PADDING,
    LAYOUT_LEFT_PADDING,
    LAYOUT_TOP_PADDING,
    LAYOUT_RIGHT_PADDING,
    LAYOUT_BOTTOM_PADDING,
    LAYOUT_NO_BORDER,
    LAYOUT_NO_ADJUST_FOR_BORDERS,
    LAYOUT_DIM,
    LAYOUT_RAISE_FOCUSED,
    LAYOUT_LOWER_WINDOWS,
    LAYOUT_TRANSFORM,
    LAYOUT_ARG,
} LayoutArgIndex;
/**
 * Options used to customize layouts
 */
typedef struct {
    /// at most limit windows will be tiled
    unsigned short limit;
    /// @{ padding between the tiled size and the actual size of the window
    short leftPadding;
    short topPadding;
    short rightPadding;
    short bottomPadding;
    /// #}
    /// if set windows won't have a border
    bool noBorder;
    /// if we set the size of the window with/without borders
    bool noAdjustForBorders;
    /// if set layouts based on X/WIDTH will use Y/HEIGHT instead
    bool dim;
    /// will raise the focused window
    bool raiseFocused;
    /// whether to raise or lower windows when tiling
    bool lowerWindows;
    /// the transformation (about the center of the monitor) to apply to all window positions
    Transform transform;
    /// generic argument
    float arg;
    /// generic argument
    float argStep;
    int getDimIndex()const {
        return dim == 0 ? CONFIG_INDEX_WIDTH : CONFIG_INDEX_HEIGHT;
    }
    int getOtherDimIndex()const {
        return dim == 0 ? CONFIG_INDEX_HEIGHT : CONFIG_INDEX_WIDTH;
    }
} LayoutArgs;

/**
 * Contains info provided to layout functions (and helper methods) that detail how the windows should be tiled
 */
typedef struct {
    /// Customized to the layout family
    LayoutArgs* args;
    /// the monitor used to layout the window
    const Monitor* monitor;
    /// number of windows that should be tiled
    const int numWindows;
    /// the stack of windows
    const ArrayList<WindowInfo*>& stack;
    LayoutArgs* getArgs() {return args;}
} LayoutState;

///holds meta data to to determine what tiling function to call and when/how to call it
typedef struct Layout {
private:
    /**
     * The name of the layout. Used solely for user
     */
    const std::string name = "";
    /**
     * The function to call to tile the windows
     */
    void (*layoutFunction)(LayoutState*) = NULL;
    /**
     * Arguments to pass into layout functions that change how windows are tiled.
     */
    LayoutArgs args = {0};
    /**
     * Used to restore args after they have been modified
     */
    LayoutArgs refArgs;
public:
    Layout(std::string name,  void (*layoutFunction)(LayoutState*), LayoutArgs args = {0}): name(name),
        layoutFunction(layoutFunction), args(args), refArgs(args) {
    }
    Layout(void (*layoutFunction)(LayoutState*), LayoutArgs args = {0}):
        layoutFunction(layoutFunction), args(args), refArgs(args) {
    }

    std::string getName()const {
        return name;
    }
    bool operator==(const Layout& l)const {return name == l.name;};
    auto getFunc() {return layoutFunction;}
    void apply(LayoutState* state) {layoutFunction(state);}
    LayoutArgs& getArgs() {return args;}
    /**
     * Saves the current args for layout so they can be restored later
     *
     */
    void saveArgs() {
        refArgs = args;
    }
    /**
     * Restores saved args for the active layout
     */
    void restoreArgs() {
        args = refArgs;
    }
} Layout;

/// Convince indexes into DEFAULT_LAYOUTS
enum {FULL, GRID, TWO_COLUMN, TWO_ROW, TWO_PANE, TWO_HPLANE, MASTER, LAST_LAYOUT};
/// ArrayList of default layouts
const ArrayList<Layout*>& getDefaultLayouts();

/**
 * Searches for the set of registered layouts for a layout with the given name and returns it
 *
 * @param name
 *
 * @return
 */
Layout* findLayoutByName(std::string name);

/**
 * Cycle through the layouts of the active workspace
 * @param dir
 */
static inline void cycleLayouts(int dir) {getActiveWorkspace()->cycleLayouts(dir);}

/**
 * Apply this external layout to the workspace if it is not already the active layout
 * @param layout
 */
static inline int toggleLayout(Layout* layout) { return getActiveWorkspace()->toggleLayout(layout);}
static inline int toggleLayout(std::string name) { return getActiveWorkspace()->toggleLayout(findLayoutByName(name));}

static inline void setActiveLayout(std::string name) {getActiveMaster()->getWorkspace()->setActiveLayout(findLayoutByName(name));}
static inline void setActiveLayout(Layout* l) {getActiveMaster()->getWorkspace()->setActiveLayout(l);}
static inline Layout* getActiveLayout() {return getActiveWorkspace()->getActiveLayout();}

ArrayList<Layout*>& getRegisteredLayouts();
/**
 * Increases the corresponding field for the layout by step
 *
 * @param index a LayoutArgIndex
 * @param step
 */
void increaseLayoutArg(int index, int step, Layout* l = getActiveWorkspace()->getActiveLayout());

/**
 * Manually retile the active workspace
 */
void retile(void);


/**
 *
 * @param windowStack the list of nodes that could possibly be tiled
 * @param args can control the max number of windows tiled (args->limit)
 * @return the number of windows that will be tiled
 */
int getNumberOfWindowsToTile(ArrayList<WindowInfo*>& windowStack, LayoutArgs* args);
static inline int getNumberOfWindowsToTile(Workspace* w) {
    return getNumberOfWindowsToTile(w->getWindowStack(), w->getActiveLayout() ? &w->getActiveLayout()->getArgs() : NULL);
}

/**
 * Provides a transformation of config
 * @param state holds info related to the layout like the monitor
 * @param config the config that will be modified
 */
void transformConfig(LayoutArgs* args, const Monitor* m, int config[CONFIG_LEN]);
/**
 * Configures the winInfo using values as reference points and apply various properties of winInfo's mask and set configuration which will override values
 * @param state
 * @param winInfo the window to tile
 * @param values where the layout wants to position the window
 */
void configureWindow(LayoutState* state, WindowInfo* winInfo, const short values[CONFIG_LEN]);

/**
 * Tiles the specified workspace.
 * First the windows in the NORMAL_LAYER are tiled according to the active layout's layoutFunction
 * (if sett) or the conditionFunction is not true then the windows are left as in.
 * Afterwards the remaining layers are tiled in ascending order
 * @param index the index of the workspace to tile
 * @see tileLowerLayers, tileUpperLayers
 */
void tileWorkspace(WorkspaceID index);

/**
 * Apply the workspace's active layout on the given workspace
 * @param workspace
 */
void applyLayout(Workspace* workspace);

/**
 * Windows will be the size of the monitor view port
 * @param state
 */
void full(LayoutState* state);

/**
 * If the number of windows to tile is 1, this method behaves like full();
 * Windows will be arranged in state->args->arg[0] columns. If arg[0] is 0 then there will log2(num windows to tile) columns
 * All columns will have the same number of windows (+/-1) and the later columns will have less
 * @param state
 */
void column(LayoutState* state);
/**
 * If the number of windows to tile is 1, this method behaves like full();
 * The head of the windowStack will take up MAX(state->args->arg[0]% of the screen, 1) and the remaining windows will be in a single column
 * @param state
 */
void masterPane(LayoutState* state);

#endif /* LAYOUTS_H_ */
