#pragma once

#include "Materials.h"
#include <SDL2/SDL.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <vector>
#include <string>
#include <map>
#include <functional>
#include <memory>

namespace PixelPhys {

// Forward declarations
class UIElement;
class DockPanel;
class ToolboxPanel;
class MaterialsPanel;
class ScenePanel;
class Canvas;

// Enums for different tools
enum class ToolType {
    Brush,      // Normal brush for placing materials
    Line,       // Line drawing tool
    Rectangle,  // Rectangle tool
    Circle,     // Circle tool
    Eraser,     // Eraser tool
    Fill,       // Bucket fill tool
    Count
};

// Base class for all UI elements with common functionality
class UIElement {
public:
    UIElement(int x, int y, int width, int height);
    virtual ~UIElement() = default;
    
    virtual void render() = 0;
    virtual bool handleEvent(const SDL_Event& event) = 0;
    virtual void update() = 0;
    
    // Positioning
    void setPosition(int x, int y);
    int getX() const { return m_bounds.x; }
    int getY() const { return m_bounds.y; }
    
    // Sizing
    void setSize(int width, int height);
    int getWidth() const { return m_bounds.w; }
    int getHeight() const { return m_bounds.h; }
    SDL_Rect getBounds() const { return m_bounds; }
    
    // Visibility
    bool isVisible() const { return m_visible; }
    void setVisible(bool visible) { m_visible = visible; }
    
    // State
    bool isHovered() const { return m_hovered; }
    bool isActive() const { return m_active; }
    
    // Parent-child relationships
    void setParent(UIElement* parent) { m_parent = parent; }
    UIElement* getParent() const { return m_parent; }
    
    // Hit testing
    bool hitTest(int x, int y) const;
    
protected:
    SDL_Rect m_bounds;
    bool m_visible = true;
    bool m_hovered = false;
    bool m_active = false;
    UIElement* m_parent = nullptr;
};

// A simple labeled button
class Button : public UIElement {
public:
    Button(int x, int y, int width, int height, const std::string& text);
    
    void render() override;
    bool handleEvent(const SDL_Event& event) override;
    void update() override;
    
    // Text
    void setText(const std::string& text) { m_text = text; }
    const std::string& getText() const { return m_text; }
    
    // Selection state
    bool isSelected() const { return m_selected; }
    void setSelected(bool selected) { m_selected = selected; }
    
    // Click handling
    using ClickCallback = std::function<void()>;
    void setOnClick(ClickCallback callback) { m_onClick = callback; }
    
private:
    std::string m_text;
    bool m_selected = false;
    bool m_pressed = false;
    ClickCallback m_onClick;
};

// A horizontal or vertical slider
class Slider : public UIElement {
public:
    enum class Orientation { Horizontal, Vertical };
    
    Slider(int x, int y, int width, int height, 
           const std::string& label, float value = 0.5f,
           Orientation orientation = Orientation::Horizontal);
    
    void render() override;
    bool handleEvent(const SDL_Event& event) override;
    void update() override;
    
    // Value
    float getValue() const { return m_value; }
    void setValue(float value);
    
    // Label
    const std::string& getLabel() const { return m_label; }
    
    // Value change callback
    using ValueCallback = std::function<void(float)>;
    void setOnValueChanged(ValueCallback callback) { m_onValueChanged = callback; }
    
private:
    std::string m_label;
    float m_value;
    Orientation m_orientation;
    SDL_Rect m_handleRect;
    bool m_dragging = false;
    ValueCallback m_onValueChanged;
    
    void updateHandlePosition();
};

// Base Panel class that can contain other UI elements
class Panel : public UIElement {
public:
    Panel(int x, int y, int width, int height, const std::string& title);
    virtual ~Panel();
    
    void render() override;
    bool handleEvent(const SDL_Event& event) override;
    void update() override;
    
    // Title
    void setTitle(const std::string& title) { m_title = title; }
    const std::string& getTitle() const { return m_title; }
    
    // Child management
    void addChild(std::unique_ptr<UIElement> child);
    void removeChild(UIElement* child);
    const std::vector<std::unique_ptr<UIElement>>& getChildren() const { return m_children; }
    
    // Panel style/behavior
    bool isResizable() const { return m_resizable; }
    void setResizable(bool resizable) { m_resizable = resizable; }
    
    bool isDraggable() const { return m_draggable; }
    void setDraggable(bool draggable) { m_draggable = draggable; }
    
    bool isCollapsible() const { return m_collapsible; }
    void setCollapsible(bool collapsible) { m_collapsible = collapsible; }
    
    bool isCollapsed() const { return m_collapsed; }
    void setCollapsed(bool collapsed);
    
protected:
    std::string m_title;
    std::vector<std::unique_ptr<UIElement>> m_children;
    
    // Panel behavior flags
    bool m_resizable = true;
    bool m_draggable = true;
    bool m_collapsible = true;
    bool m_collapsed = false;
    
    // Drag state
    bool m_dragging = false;
    int m_dragOffsetX = 0;
    int m_dragOffsetY = 0;
    
    // Resize state
    bool m_resizing = false;
    int m_resizeEdge = 0; // 0=none, 1=left, 2=right, 3=top, 4=bottom
    
    // Constants
    static constexpr int TITLE_HEIGHT = 25;
    static constexpr int RESIZE_HANDLE_SIZE = 8;
    
public:
    // Public constants for external use
    static constexpr int PANEL_TITLE_HEIGHT = TITLE_HEIGHT;
    
    // Helper rendering methods
    virtual void renderBackground();
    virtual void renderTitleBar();
    virtual void renderBorder();
    virtual void renderChildren();
    
    // Hit test the resize handles
    int hitTestResizeHandles(int x, int y);
};

// A dock panel that can contain dockable panels
class DockPanel : public Panel {
public:
    enum class DockPosition { Left, Right, Top, Bottom, Center };
    
    DockPanel(int x, int y, int width, int height);
    
    // Dock a panel at the specified position
    void dockPanel(std::unique_ptr<Panel> panel, DockPosition position);
    
    // Override rendering to handle docked panels
    void render() override;
    bool handleEvent(const SDL_Event& event) override;
    void update() override;
    
private:
    std::map<DockPosition, std::unique_ptr<Panel>> m_dockedPanels;
    std::vector<DockPosition> m_dockOrder;
    
    // Calculate layout of docked panels
    void recalculateLayout();
};

// The main scene view panel showing the simulation
class ScenePanel : public Panel {
public:
    ScenePanel(int x, int y, int width, int height);
    
    void render() override;
    bool handleEvent(const SDL_Event& event) override;
    void update() override;
    
    // Camera controls
    void setCameraPosition(int x, int y);
    void getCameraPosition(int& x, int& y) const;
    float getZoomLevel() const { return m_zoomLevel; }
    void setZoomLevel(float zoom);
    
    // World position conversion
    void screenToWorld(int screenX, int screenY, int& worldX, int& worldY) const;
    void worldToScreen(int worldX, int worldY, int& screenX, int& screenY) const;
    
    // Simulation state
    bool isSimulationRunning() const { return m_simulationRunning; }
    void setSimulationRunning(bool running) { m_simulationRunning = running; }
    
private:
    // Camera state
    int m_cameraX = 0;
    int m_cameraY = 0;
    float m_zoomLevel = 1.0f;
    
    // Interaction state
    bool m_panning = false;
    int m_lastMouseX = 0;
    int m_lastMouseY = 0;
    
    // Simulation control
    bool m_simulationRunning = false;
    
    // Helper methods for coordinate conversion
    void setupViewTransform();
};

// Panel for material selection
class MaterialsPanel : public Panel {
public:
    MaterialsPanel(int x, int y, int width, int height);
    
    MaterialType getSelectedMaterial() const { return m_selectedMaterial; }
    
private:
    MaterialType m_selectedMaterial = MaterialType::Sand;
    std::vector<Button*> m_materialButtons;
    
    void initializeMaterials();
    void selectMaterial(MaterialType material);
};

// Panel for tool selection and properties
class ToolboxPanel : public Panel {
public:
    ToolboxPanel(int x, int y, int width, int height);
    
    ToolType getSelectedTool() const { return m_selectedTool; }
    int getBrushSize() const { return m_brushSize; }
    
private:
    ToolType m_selectedTool = ToolType::Brush;
    int m_brushSize = 5;
    std::vector<Button*> m_toolButtons;
    Slider* m_brushSizeSlider = nullptr;
    
    void initializeTools();
    void selectTool(ToolType tool);
};

// Main UI manager class
class UI {
public:
    UI(int screenWidth, int screenHeight);
    ~UI();
    
    // Initialization
    bool initialize();
    void cleanup();
    
    // Main UI loop methods
    bool handleEvent(const SDL_Event& event);
    void update();
    void render();
    
    // Window management
    void resize(int width, int height);
    void toggleFullscreen(SDL_Window* window);
    
    // UI state accessors
    MaterialType getSelectedMaterial() const;
    ToolType getSelectedTool() const;
    int getBrushSize() const;
    float getZoomLevel() const;
    void getCameraPosition(int& x, int& y) const;
    SDL_Rect getScenePanelRect() const;
    bool isSimulationRunning() const;
    
    // Status and FPS display
    void setFPS(int fps) { m_fps = fps; }
    void setStatusText(const std::string& text) { m_statusText = text; }
    
    // Static helper methods for rendering
    static void drawText(const std::string& text, int x, int y, const SDL_Color& color);
    static void drawRect(const SDL_Rect& rect, const SDL_Color& color, bool filled = true);
    
private:
    // Screen dimensions
    int m_screenWidth;
    int m_screenHeight;
    
    // UI state
    bool m_initialized = false;
    bool m_isFullscreen = false;
    int m_fps = 0;
    std::string m_statusText;
    
    // Root UI container
    std::unique_ptr<DockPanel> m_rootPanel;
    
    // Main UI panels (owned by m_rootPanel)
    ScenePanel* m_scenePanel = nullptr;
    MaterialsPanel* m_materialsPanel = nullptr;
    ToolboxPanel* m_toolboxPanel = nullptr;
    
    // Layout calculation
    void calculateLayout();
};

} // namespace PixelPhys