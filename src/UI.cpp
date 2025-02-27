#include "../include/UI.h"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>

namespace PixelPhys {

//
// UIElement implementation
//
UIElement::UIElement(int x, int y, int width, int height)
    : m_bounds{x, y, width, height}
{
}

bool UIElement::hitTest(int x, int y) const {
    if (!m_visible) return false;
    
    return (x >= m_bounds.x && x < m_bounds.x + m_bounds.w &&
            y >= m_bounds.y && y < m_bounds.y + m_bounds.h);
}

void UIElement::setPosition(int x, int y) {
    m_bounds.x = x;
    m_bounds.y = y;
}

void UIElement::setSize(int width, int height) {
    m_bounds.w = width;
    m_bounds.h = height;
}

//
// Button implementation
//
Button::Button(int x, int y, int width, int height, const std::string& text)
    : UIElement(x, y, width, height)
    , m_text(text)
{
}

void Button::render() {
    if (!m_visible) return;
    
    // Draw button background based on state
    SDL_Color bgColor;
    
    if (m_pressed) {
        bgColor = {60, 60, 180, 255}; // Darker blue when pressed
    } else if (m_selected) {
        bgColor = {100, 150, 250, 255}; // Brighter blue when selected
    } else if (m_hovered) {
        bgColor = {80, 120, 220, 255}; // Medium blue when hovered
    } else {
        bgColor = {70, 70, 80, 255}; // Dark gray-blue in normal state
    }
    
    // Draw button with a simple border
    UI::drawRect(m_bounds, bgColor, true);
    
    // Draw a border (lighter for selected buttons)
    SDL_Color borderColor = m_selected ? 
        SDL_Color{180, 200, 255, 255} : SDL_Color{40, 40, 60, 255};
    
    SDL_Rect borderRect = {
        m_bounds.x - 1,
        m_bounds.y - 1,
        m_bounds.w + 2,
        m_bounds.h + 2
    };
    
    UI::drawRect(borderRect, borderColor, false);
    
    // Draw text label
    SDL_Color textColor = {230, 230, 240, 255}; // White-ish
    UI::drawText(m_text, m_bounds.x + m_bounds.w/2 - 4*m_text.length(), 
                 m_bounds.y + m_bounds.h/2 - 6, textColor);
}

bool Button::handleEvent(const SDL_Event& event) {
    if (!m_visible) return false;
    
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            bool wasHovered = m_hovered;
            m_hovered = hitTest(event.motion.x, event.motion.y);
            
            // If hover state changed, return true to trigger redraw
            return (wasHovered != m_hovered);
        }
        
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT && 
                hitTest(event.button.x, event.button.y)) {
                m_pressed = true;
                return true;
            }
            break;
            
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT && m_pressed) {
                m_pressed = false;
                
                // Check if the mouse is still over the button
                if (hitTest(event.button.x, event.button.y) && m_onClick) {
                    m_onClick(); // Execute the callback
                }
                
                return true;
            }
            break;
    }
    
    return false;
}

void Button::update() {
    // Nothing to update for a basic button
}

//
// Slider implementation
//
Slider::Slider(int x, int y, int width, int height, 
               const std::string& label, float value, 
               Orientation orientation)
    : UIElement(x, y, width, height)
    , m_label(label)
    , m_value(std::max(0.0f, std::min(1.0f, value)))
    , m_orientation(orientation)
    , m_handleRect{0, 0, 0, 0}
{
    // Set up the handle size based on orientation
    if (m_orientation == Orientation::Horizontal) {
        m_handleRect.w = height - 4;
        m_handleRect.h = height - 4;
    } else {
        m_handleRect.w = width - 4;
        m_handleRect.h = width - 4;
    }
    
    updateHandlePosition();
}

void Slider::render() {
    if (!m_visible) return;
    
    // Draw background track
    SDL_Color trackColor = {50, 50, 60, 255};
    UI::drawRect(m_bounds, trackColor, true);
    
    // Draw filled portion of the track
    SDL_Rect filledRect = m_bounds;
    if (m_orientation == Orientation::Horizontal) {
        filledRect.w = static_cast<int>(m_bounds.w * m_value);
    } else {
        int filledHeight = static_cast<int>(m_bounds.h * m_value);
        filledRect.y = m_bounds.y + m_bounds.h - filledHeight;
        filledRect.h = filledHeight;
    }
    
    SDL_Color filledColor = {80, 120, 200, 255};
    UI::drawRect(filledRect, filledColor, true);
    
    // Draw handle
    SDL_Color handleColor = m_dragging ? 
        SDL_Color{120, 160, 240, 255} : // Brighter when dragging
        SDL_Color{90, 140, 210, 255};  // Normal handle color
    
    UI::drawRect(m_handleRect, handleColor, true);
    
    // Draw label (position depends on orientation)
    SDL_Color textColor = {200, 200, 200, 255};
    if (m_orientation == Orientation::Horizontal) {
        UI::drawText(m_label, m_bounds.x, m_bounds.y - 20, textColor);
        
        // Draw value as percentage
        std::string valueText = std::to_string(static_cast<int>(m_value * 100)) + "%";
        UI::drawText(valueText, m_bounds.x + m_bounds.w + 5, m_bounds.y + 2, textColor);
    } else {
        UI::drawText(m_label, m_bounds.x, m_bounds.y - 20, textColor);
        
        // Draw value as percentage
        std::string valueText = std::to_string(static_cast<int>(m_value * 100)) + "%";
        UI::drawText(valueText, m_bounds.x, m_bounds.y + m_bounds.h + 5, textColor);
    }
}

bool Slider::handleEvent(const SDL_Event& event) {
    if (!m_visible) return false;
    
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            int mx = event.motion.x;
            int my = event.motion.y;
            
            // Handle dragging
            if (m_dragging) {
                if (m_orientation == Orientation::Horizontal) {
                    float newValue = static_cast<float>(mx - m_bounds.x) / static_cast<float>(m_bounds.w);
                    setValue(newValue);
                } else {
                    float newValue = 1.0f - static_cast<float>(my - m_bounds.y) / static_cast<float>(m_bounds.h);
                    setValue(newValue);
                }
                
                // Call the callback if set
                if (m_onValueChanged) {
                    m_onValueChanged(m_value);
                }
                
                return true;
            }
            
            // Update hover state
            bool wasHovered = m_hovered;
            m_hovered = hitTest(mx, my);
            
            return (wasHovered != m_hovered); // Return true if hover state changed
        }
        
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                
                // Check if the click is inside the slider
                if (hitTest(mx, my)) {
                    m_dragging = true;
                    
                    // Update value based on click position
                    if (m_orientation == Orientation::Horizontal) {
                        float newValue = static_cast<float>(mx - m_bounds.x) / static_cast<float>(m_bounds.w);
                        setValue(newValue);
                    } else {
                        float newValue = 1.0f - static_cast<float>(my - m_bounds.y) / static_cast<float>(m_bounds.h);
                        setValue(newValue);
                    }
                    
                    // Call the callback if set
                    if (m_onValueChanged) {
                        m_onValueChanged(m_value);
                    }
                    
                    return true;
                }
            }
            break;
            
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT && m_dragging) {
                m_dragging = false;
                return true;
            }
            break;
    }
    
    return false;
}

void Slider::update() {
    // Nothing to update for a basic slider
}

void Slider::setValue(float value) {
    // Clamp value between 0 and 1
    m_value = std::max(0.0f, std::min(1.0f, value));
    updateHandlePosition();
}

void Slider::updateHandlePosition() {
    // Position the handle based on the current value and orientation
    if (m_orientation == Orientation::Horizontal) {
        m_handleRect.x = m_bounds.x + static_cast<int>((m_bounds.w - m_handleRect.w) * m_value);
        m_handleRect.y = m_bounds.y + (m_bounds.h - m_handleRect.h) / 2;
    } else {
        m_handleRect.x = m_bounds.x + (m_bounds.w - m_handleRect.w) / 2;
        m_handleRect.y = m_bounds.y + static_cast<int>((m_bounds.h - m_handleRect.h) * (1.0f - m_value));
    }
}

//
// Panel implementation
//
Panel::Panel(int x, int y, int width, int height, const std::string& title)
    : UIElement(x, y, width, height)
    , m_title(title)
{
}

Panel::~Panel() {
    // Panel owns its children and should delete them
    m_children.clear();
}

void Panel::render() {
    if (!m_visible) return;
    
    // Don't render if collapsed, except for the title bar
    renderBackground();
    renderBorder();
    renderTitleBar();
    
    if (!m_collapsed) {
        renderChildren();
    }
}

void Panel::renderBackground() {
    // Draw panel background with semi-transparency
    SDL_Color bgColor = {30, 30, 40, 220};
    UI::drawRect(m_bounds, bgColor, true);
}

void Panel::renderBorder() {
    // Draw panel border
    SDL_Color borderColor = m_hovered ? 
        SDL_Color{100, 140, 220, 255} : // Highlight when hovered
        SDL_Color{60, 60, 80, 255};     // Normal border
    
    SDL_Rect borderRect = {
        m_bounds.x - 1,
        m_bounds.y - 1,
        m_bounds.w + 2,
        m_bounds.h + 2
    };
    
    UI::drawRect(borderRect, borderColor, false);
}

void Panel::renderTitleBar() {
    // Draw title bar at the top of the panel
    SDL_Rect titleBarRect = {
        m_bounds.x,
        m_bounds.y,
        m_bounds.w,
        TITLE_HEIGHT
    };
    
    // Title bar color
    SDL_Color titleBgColor = m_dragging ?
        SDL_Color{80, 100, 180, 250} : // Highlight when dragging
        SDL_Color{50, 70, 120, 230};   // Normal title bar
    
    UI::drawRect(titleBarRect, titleBgColor, true);
    
    // Draw title text
    SDL_Color titleTextColor = {230, 230, 240, 255};
    UI::drawText(m_title, m_bounds.x + 10, m_bounds.y + TITLE_HEIGHT/2 - 7, titleTextColor);
    
    // Draw collapse/expand button if collapsible
    if (m_collapsible) {
        SDL_Rect collapseButtonRect = {
            m_bounds.x + m_bounds.w - 20,
            m_bounds.y + 5,
            15,
            15
        };
        
        // Button background
        SDL_Color buttonColor = {70, 90, 140, 255};
        UI::drawRect(collapseButtonRect, buttonColor, true);
        
        // Draw "-" or "+" symbol
        SDL_Color symbolColor = {220, 220, 240, 255};
        if (m_collapsed) {
            // Draw "+"
            SDL_Rect vLine = {
                collapseButtonRect.x + 7,
                collapseButtonRect.y + 3,
                2,
                10
            };
            UI::drawRect(vLine, symbolColor, true);
        }
        
        // Always draw horizontal line
        SDL_Rect hLine = {
            collapseButtonRect.x + 3,
            collapseButtonRect.y + 7,
            10,
            2
        };
        UI::drawRect(hLine, symbolColor, true);
    }
}

void Panel::renderChildren() {
    for (const auto& child : m_children) {
        if (child->isVisible()) {
            child->render();
        }
    }
}

bool Panel::handleEvent(const SDL_Event& event) {
    if (!m_visible) return false;
    
    // Process mouse events for panel behaviors
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            int mx = event.motion.x;
            int my = event.motion.y;
            
            // Update hover state
            bool wasHovered = m_hovered;
            
            // We're hovered if the mouse is over any part of us
            m_hovered = hitTest(mx, my);
            
            // Handle dragging
            if (m_dragging && m_draggable) {
                setPosition(mx - m_dragOffsetX, my - m_dragOffsetY);
                return true;
            }
            
            // Handle resizing
            if (m_resizing && m_resizable) {
                // Different resize logic based on which edge we're dragging
                switch (m_resizeEdge) {
                    case 1: // Left edge
                        {
                            int newX = mx;
                            int newWidth = m_bounds.x + m_bounds.w - newX;
                            
                            if (newWidth >= 50) { // Minimum size constraint
                                setPosition(newX, m_bounds.y);
                                setSize(newWidth, m_bounds.h);
                            }
                        }
                        break;
                        
                    case 2: // Right edge
                        {
                            int newWidth = mx - m_bounds.x;
                            if (newWidth >= 50) {
                                setSize(newWidth, m_bounds.h);
                            }
                        }
                        break;
                        
                    case 3: // Top edge
                        {
                            int newY = my;
                            int newHeight = m_bounds.y + m_bounds.h - newY;
                            
                            if (newHeight >= 50) {
                                setPosition(m_bounds.x, newY);
                                setSize(m_bounds.w, newHeight);
                            }
                        }
                        break;
                        
                    case 4: // Bottom edge
                        {
                            int newHeight = my - m_bounds.y;
                            if (newHeight >= 50) {
                                setSize(m_bounds.w, newHeight);
                            }
                        }
                        break;
                }
                
                return true;
            }
            
            // Update cursor based on resize edge
            if (m_resizable && !m_collapsed) {
                int resizeEdge = hitTestResizeHandles(mx, my);
                // Actually update cursor here in a real implementation
                
                if (resizeEdge != 0) {
                    return true; // We're over a resize handle
                }
            }
            
            // If we're not collapsed, pass the event to children
            if (!m_collapsed) {
                // Process the event in reverse order (top-most child first)
                for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
                    if ((*it)->handleEvent(event)) {
                        return true; // Event was handled by a child
                    }
                }
            }
            
            // Return true if our hover state changed
            return (wasHovered != m_hovered);
        }
        
        case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) {
                int mx = event.button.x;
                int my = event.button.y;
                
                // Check for clicks on title bar or resize handles
                SDL_Rect titleBarRect = {
                    m_bounds.x, m_bounds.y, m_bounds.w, TITLE_HEIGHT
                };
                
                // Check for collapse button click
                if (m_collapsible) {
                    SDL_Rect collapseButtonRect = {
                        m_bounds.x + m_bounds.w - 20,
                        m_bounds.y + 5,
                        15,
                        15
                    };
                    
                    if (mx >= collapseButtonRect.x && mx < collapseButtonRect.x + collapseButtonRect.w &&
                        my >= collapseButtonRect.y && my < collapseButtonRect.y + collapseButtonRect.h) {
                        
                        // Toggle collapsed state
                        setCollapsed(!m_collapsed);
                        return true;
                    }
                }
                
                // Check for title bar drag
                bool hitTitleBar = mx >= titleBarRect.x && mx < titleBarRect.x + titleBarRect.w &&
                                  my >= titleBarRect.y && my < titleBarRect.y + titleBarRect.h;
                
                if (hitTitleBar && m_draggable) {
                    // Start dragging
                    m_dragging = true;
                    m_dragOffsetX = mx - m_bounds.x;
                    m_dragOffsetY = my - m_bounds.y;
                    return true;
                }
                
                // Check for resize handle hit
                if (m_resizable && !m_collapsed) {
                    int resizeEdge = hitTestResizeHandles(mx, my);
                    if (resizeEdge != 0) {
                        m_resizing = true;
                        m_resizeEdge = resizeEdge;
                        return true;
                    }
                }
                
                // If we're not collapsed, pass the event to children
                if (!m_collapsed) {
                    // Process the event in reverse order (top-most child first)
                    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
                        if ((*it)->handleEvent(event)) {
                            return true; // Event was handled by a child
                        }
                    }
                }
                
                // If we got here, the click was on the panel but not on a child or control element
                return hitTest(mx, my);
            }
            break;
            
        case SDL_MOUSEBUTTONUP:
            if (event.button.button == SDL_BUTTON_LEFT) {
                bool wasActive = m_dragging || m_resizing;
                m_dragging = false;
                m_resizing = false;
                
                // If we're not collapsed, pass the event to children
                if (!m_collapsed) {
                    // Process the event in reverse order (top-most child first)
                    for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
                        if ((*it)->handleEvent(event)) {
                            return true; // Event was handled by a child
                        }
                    }
                }
                
                // Return true if we were active (to trigger a redraw)
                return wasActive;
            }
            break;
            
        default:
            // For other events, pass to children if not collapsed
            if (!m_collapsed) {
                for (auto it = m_children.rbegin(); it != m_children.rend(); ++it) {
                    if ((*it)->handleEvent(event)) {
                        return true; // Event was handled by a child
                    }
                }
            }
            break;
    }
    
    return false;
}

void Panel::update() {
    // Update all visible children
    if (!m_collapsed) {
        for (auto& child : m_children) {
            if (child->isVisible()) {
                child->update();
            }
        }
    }
}

int Panel::hitTestResizeHandles(int x, int y) {
    if (!m_resizable || m_collapsed) return 0;
    
    const int handleSize = RESIZE_HANDLE_SIZE;
    
    // Left edge
    if (x >= m_bounds.x - handleSize/2 && x < m_bounds.x + handleSize/2 &&
        y >= m_bounds.y + TITLE_HEIGHT && y < m_bounds.y + m_bounds.h) {
        return 1;
    }
    
    // Right edge
    if (x >= m_bounds.x + m_bounds.w - handleSize/2 && x < m_bounds.x + m_bounds.w + handleSize/2 &&
        y >= m_bounds.y + TITLE_HEIGHT && y < m_bounds.y + m_bounds.h) {
        return 2;
    }
    
    // Top edge
    if (y >= m_bounds.y + TITLE_HEIGHT - handleSize/2 && y < m_bounds.y + TITLE_HEIGHT + handleSize/2 &&
        x >= m_bounds.x && x < m_bounds.x + m_bounds.w) {
        return 3;
    }
    
    // Bottom edge
    if (y >= m_bounds.y + m_bounds.h - handleSize/2 && y < m_bounds.y + m_bounds.h + handleSize/2 &&
        x >= m_bounds.x && x < m_bounds.x + m_bounds.w) {
        return 4;
    }
    
    return 0; // No hit
}

void Panel::setCollapsed(bool collapsed) {
    if (m_collapsible) {
        m_collapsed = collapsed;
    }
}

void Panel::addChild(std::unique_ptr<UIElement> child) {
    child->setParent(this);
    m_children.push_back(std::move(child));
}

void Panel::removeChild(UIElement* child) {
    auto it = std::find_if(m_children.begin(), m_children.end(),
                         [child](const std::unique_ptr<UIElement>& c) {
                             return c.get() == child;
                         });
    
    if (it != m_children.end()) {
        m_children.erase(it);
    }
}

//
// DockPanel implementation
//
DockPanel::DockPanel(int x, int y, int width, int height)
    : Panel(x, y, width, height, "Root")
{
    // Set up default dock order
    m_dockOrder = {
        DockPosition::Left,
        DockPosition::Right,
        DockPosition::Top,
        DockPosition::Bottom,
        DockPosition::Center
    };
    
    // The root panel is not draggable, resizable, or collapsible
    m_draggable = false;
    m_resizable = false;
    m_collapsible = false;
}

void DockPanel::dockPanel(std::unique_ptr<Panel> panel, DockPosition position) {
    panel->setParent(this);
    m_dockedPanels[position] = std::move(panel);
    recalculateLayout();
}

void DockPanel::render() {
    if (!m_visible) return;
    
    // Render the dock panel background
    renderBackground();
    
    // Render each docked panel in order
    for (auto pos : m_dockOrder) {
        auto it = m_dockedPanels.find(pos);
        if (it != m_dockedPanels.end() && it->second->isVisible()) {
            it->second->render();
        }
    }
}

bool DockPanel::handleEvent(const SDL_Event& event) {
    if (!m_visible) return false;
    
    // Process the event in reverse dock order (top-most panel first)
    for (auto it = m_dockOrder.rbegin(); it != m_dockOrder.rend(); ++it) {
        auto panelIt = m_dockedPanels.find(*it);
        if (panelIt != m_dockedPanels.end() && panelIt->second->isVisible()) {
            if (panelIt->second->handleEvent(event)) {
                return true; // Event was handled
            }
        }
    }
    
    // If we get here, no docked panel handled the event
    return false;
}

void DockPanel::update() {
    // Update all docked panels
    for (auto& [pos, panel] : m_dockedPanels) {
        if (panel->isVisible()) {
            panel->update();
        }
    }
}

void DockPanel::recalculateLayout() {
    // Default sizes for docked panels (percentage of available space)
    const float LEFT_WIDTH_PCT = 0.2f;   // 20% of width for left panel
    const float RIGHT_WIDTH_PCT = 0.2f;  // 20% of width for right panel
    const float TOP_HEIGHT_PCT = 0.1f;   // 10% of height for top panel
    const float BOTTOM_HEIGHT_PCT = 0.1f; // 10% of height for bottom panel
    
    // Calculate sizes (these would normally be configurable)
    int leftWidth = static_cast<int>(m_bounds.w * LEFT_WIDTH_PCT);
    int rightWidth = static_cast<int>(m_bounds.w * RIGHT_WIDTH_PCT);
    int topHeight = static_cast<int>(m_bounds.h * TOP_HEIGHT_PCT);
    int bottomHeight = static_cast<int>(m_bounds.h * BOTTOM_HEIGHT_PCT);
    
    // Position each docked panel
    for (auto& [pos, panel] : m_dockedPanels) {
        switch (pos) {
            case DockPosition::Left:
                panel->setPosition(m_bounds.x, m_bounds.y);
                panel->setSize(leftWidth, m_bounds.h);
                break;
                
            case DockPosition::Right:
                panel->setPosition(m_bounds.x + m_bounds.w - rightWidth, m_bounds.y);
                panel->setSize(rightWidth, m_bounds.h);
                break;
                
            case DockPosition::Top:
                panel->setPosition(m_bounds.x + leftWidth, m_bounds.y);
                panel->setSize(m_bounds.w - leftWidth - rightWidth, topHeight);
                break;
                
            case DockPosition::Bottom:
                panel->setPosition(m_bounds.x + leftWidth, m_bounds.y + m_bounds.h - bottomHeight);
                panel->setSize(m_bounds.w - leftWidth - rightWidth, bottomHeight);
                break;
                
            case DockPosition::Center:
                panel->setPosition(m_bounds.x + leftWidth, m_bounds.y + topHeight);
                panel->setSize(m_bounds.w - leftWidth - rightWidth, 
                              m_bounds.h - topHeight - bottomHeight);
                break;
        }
    }
}

//
// ScenePanel implementation
//
ScenePanel::ScenePanel(int x, int y, int width, int height)
    : Panel(x, y, width, height, "Scene")
{
}

void ScenePanel::render() {
    // Call base class to render the panel background, border, etc.
    Panel::render();
    
    if (m_collapsed) return;
    
    // This is where we would render the world view
    // For now, just draw a placeholder grid
    
    // Set up the view transform for the scene
    setupViewTransform();
    
    // Draw a grid to visualize the world space
    const int gridSize = 50; // Grid cell size in world units
    const int gridLines = 20; // Number of grid lines in each direction
    
    // Calculate grid bounds
    int minX = -gridLines * gridSize / 2;
    int maxX = gridLines * gridSize / 2;
    int minY = -gridLines * gridSize / 2;
    int maxY = gridLines * gridSize / 2;
    
    // Draw faint grid lines
    SDL_Color gridColor = {80, 80, 100, 100}; // Light gray, semi-transparent
    
    // Draw horizontal grid lines
    for (int y = minY; y <= maxY; y += gridSize) {
        SDL_Rect line = {minX, y, maxX - minX, 1};
        UI::drawRect(line, gridColor, true);
    }
    
    // Draw vertical grid lines
    for (int x = minX; x <= maxX; x += gridSize) {
        SDL_Rect line = {x, minY, 1, maxY - minY};
        UI::drawRect(line, gridColor, true);
    }
    
    // Draw axes with more emphasis
    SDL_Color axisColor = {120, 140, 200, 200}; // Blueish axes
    
    // X axis
    SDL_Rect xAxis = {minX, 0, maxX - minX, 2};
    UI::drawRect(xAxis, axisColor, true);
    
    // Y axis
    SDL_Rect yAxis = {0, minY, 2, maxY - minY};
    UI::drawRect(yAxis, axisColor, true);
    
    // Draw origin marker
    SDL_Rect origin = {-5, -5, 10, 10};
    SDL_Color originColor = {200, 100, 100, 255}; // Red-ish
    UI::drawRect(origin, originColor, true);
    
    // Draw camera info
    SDL_Color textColor = {220, 220, 240, 255};
    std::string cameraInfo = "Camera: (" + std::to_string(m_cameraX) + ", " + 
                           std::to_string(m_cameraY) + ") Zoom: " + 
                           std::to_string(m_zoomLevel);
    
    // This would normally convert from world to screen coords
    UI::drawText(cameraInfo, m_bounds.x + 10, m_bounds.y + 30, textColor);
    
    // Reset the OpenGL matrix stack
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

bool ScenePanel::handleEvent(const SDL_Event& event) {
    // Let the panel handle basic interactions first
    if (Panel::handleEvent(event)) {
        return true;
    }
    
    // Additional scene-specific interaction handling
    if (!m_visible || m_collapsed) return false;
    
    switch (event.type) {
        case SDL_MOUSEMOTION: {
            int mx = event.motion.x;
            int my = event.motion.y;
            
            // Only handle events inside the scene content area
            if (!hitTest(mx, my) || my < m_bounds.y + TITLE_HEIGHT) {
                return false;
            }
            
            // Handle panning with middle mouse button
            if (m_panning) {
                // Calculate the delta in screen coordinates
                int dx = mx - m_lastMouseX;
                int dy = my - m_lastMouseY;
                
                // Convert screen delta to world delta based on zoom
                float worldDx = dx / m_zoomLevel;
                float worldDy = dy / m_zoomLevel;
                
                // Pan the camera in the opposite direction of mouse movement
                m_cameraX -= static_cast<int>(worldDx);
                m_cameraY -= static_cast<int>(worldDy);
                
                m_lastMouseX = mx;
                m_lastMouseY = my;
                return true;
            }
            
            // Store current mouse position for future interactions
            m_lastMouseX = mx;
            m_lastMouseY = my;
            break;
        }
        
        case SDL_MOUSEBUTTONDOWN: {
            int mx = event.button.x;
            int my = event.button.y;
            
            // Only handle events inside the scene content area
            if (!hitTest(mx, my) || my < m_bounds.y + TITLE_HEIGHT) {
                return false;
            }
            
            // Middle mouse button for panning
            if (event.button.button == SDL_BUTTON_MIDDLE) {
                m_panning = true;
                m_lastMouseX = mx;
                m_lastMouseY = my;
                return true;
            }
            
            break;
        }
        
        case SDL_MOUSEBUTTONUP: {
            // Stop panning on middle mouse button release
            if (event.button.button == SDL_BUTTON_MIDDLE) {
                m_panning = false;
                return true;
            }
            break;
        }
        
        case SDL_MOUSEWHEEL: {
            int mx, my;
            SDL_GetMouseState(&mx, &my);
            
            // Only handle zoom if mouse is inside scene content area
            if (!hitTest(mx, my) || my < m_bounds.y + TITLE_HEIGHT) {
                return false;
            }
            
            // Zoom in/out around the mouse position
            float zoomDelta = event.wheel.y > 0 ? 0.1f : -0.1f;
            float newZoom = m_zoomLevel * (1.0f + zoomDelta);
            
            // Clamp zoom level
            newZoom = std::max(0.1f, std::min(10.0f, newZoom));
            
            // Apply new zoom level
            setZoomLevel(newZoom);
            return true;
        }
    }
    
    return false;
}

void ScenePanel::update() {
    Panel::update();
    
    // Additional scene-specific updates would go here
}

void ScenePanel::setCameraPosition(int x, int y) {
    m_cameraX = x;
    m_cameraY = y;
}

void ScenePanel::getCameraPosition(int& x, int& y) const {
    x = m_cameraX;
    y = m_cameraY;
}

void ScenePanel::setZoomLevel(float zoom) {
    m_zoomLevel = std::max(0.1f, std::min(10.0f, zoom));
}

void ScenePanel::setupViewTransform() {
    // Save the current projection and modelview matrices
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    
    // Set up orthographic projection for the scene view
    // We subtract TITLE_HEIGHT from the panel height to account for the title bar
    int contentHeight = m_bounds.h - TITLE_HEIGHT;
    glOrtho(m_bounds.x, m_bounds.x + m_bounds.w,
            m_bounds.y + m_bounds.h, m_bounds.y + TITLE_HEIGHT, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Apply transformations to set up the scene view
    
    // 1. Translate to the center of the panel's content area
    int contentCenterX = m_bounds.x + m_bounds.w / 2;
    int contentCenterY = m_bounds.y + TITLE_HEIGHT + contentHeight / 2;
    glTranslatef(contentCenterX, contentCenterY, 0.0f);
    
    // 2. Apply zoom
    glScalef(m_zoomLevel, m_zoomLevel, 1.0f);
    
    // 3. Apply camera position (negate because we're moving the world, not the camera)
    glTranslatef(-m_cameraX, -m_cameraY, 0.0f);
}

void ScenePanel::screenToWorld(int screenX, int screenY, int& worldX, int& worldY) const {
    // Make sure screen position is within panel content area
    if (screenX < m_bounds.x || screenX >= m_bounds.x + m_bounds.w ||
        screenY < m_bounds.y + TITLE_HEIGHT || screenY >= m_bounds.y + m_bounds.h) {
        worldX = worldY = 0;
        return;
    }
    
    // Calculate the center of the content area
    int contentWidth = m_bounds.w;
    int contentHeight = m_bounds.h - TITLE_HEIGHT;
    int contentCenterX = m_bounds.x + contentWidth / 2;
    int contentCenterY = m_bounds.y + TITLE_HEIGHT + contentHeight / 2;
    
    // Calculate offset from center in screen space
    int screenOffsetX = screenX - contentCenterX;
    int screenOffsetY = screenY - contentCenterY;
    
    // Convert to world space
    float worldOffsetX = screenOffsetX / m_zoomLevel;
    float worldOffsetY = screenOffsetY / m_zoomLevel;
    
    // Apply camera position
    worldX = static_cast<int>(m_cameraX + worldOffsetX);
    worldY = static_cast<int>(m_cameraY + worldOffsetY);
}

void ScenePanel::worldToScreen(int worldX, int worldY, int& screenX, int& screenY) const {
    // Calculate the center of the content area
    int contentWidth = m_bounds.w;
    int contentHeight = m_bounds.h - TITLE_HEIGHT;
    int contentCenterX = m_bounds.x + contentWidth / 2;
    int contentCenterY = m_bounds.y + TITLE_HEIGHT + contentHeight / 2;
    
    // Calculate world offset from camera
    float worldOffsetX = worldX - m_cameraX;
    float worldOffsetY = worldY - m_cameraY;
    
    // Convert to screen space
    int screenOffsetX = static_cast<int>(worldOffsetX * m_zoomLevel);
    int screenOffsetY = static_cast<int>(worldOffsetY * m_zoomLevel);
    
    // Apply content center
    screenX = contentCenterX + screenOffsetX;
    screenY = contentCenterY + screenOffsetY;
}

//
// MaterialsPanel implementation
//
MaterialsPanel::MaterialsPanel(int x, int y, int width, int height)
    : Panel(x, y, width, height, "Materials")
{
    initializeMaterials();
}

void MaterialsPanel::initializeMaterials() {
    // Create buttons for each material type
    const int BUTTON_HEIGHT = 30;
    const int BUTTON_WIDTH = m_bounds.w - 20; // 10px padding on each side
    const int SPACING = 10;
    const int START_Y = TITLE_HEIGHT + 10;
    
    // Material names and default selections
    struct MaterialInfo {
        MaterialType type;
        std::string name;
        bool defaultSelected;
    };
    
    MaterialInfo materials[] = {
        {MaterialType::Empty, "Eraser", false},
        {MaterialType::Sand, "Sand", true},
        {MaterialType::Water, "Water", false},
        {MaterialType::Stone, "Stone", false},
        {MaterialType::Wood, "Wood", false},
        {MaterialType::Fire, "Fire", false},
        {MaterialType::Oil, "Oil", false}
    };
    
    int index = 0;
    for (const auto& material : materials) {
        int y = START_Y + index * (BUTTON_HEIGHT + SPACING);
        
        auto button = std::make_unique<Button>(
            m_bounds.x + 10, y, BUTTON_WIDTH, BUTTON_HEIGHT, material.name
        );
        
        // Set initial selection state
        button->setSelected(material.defaultSelected);
        if (material.defaultSelected) {
            m_selectedMaterial = material.type;
        }
        
        // Capture material.type by value for the lambda
        MaterialType type = material.type;
        button->setOnClick([this, type]() {
            selectMaterial(type);
        });
        
        m_materialButtons.push_back(button.get());
        addChild(std::move(button));
        
        index++;
    }
}

void MaterialsPanel::selectMaterial(MaterialType material) {
    m_selectedMaterial = material;
    
    // Update button selection states
    size_t index = 0;
    for (auto button : m_materialButtons) {
        button->setSelected(index == static_cast<size_t>(material));
        index++;
    }
}

//
// ToolboxPanel implementation
//
ToolboxPanel::ToolboxPanel(int x, int y, int width, int height)
    : Panel(x, y, width, height, "Tools")
{
    initializeTools();
}

void ToolboxPanel::initializeTools() {
    // Create buttons for each tool type
    const int BUTTON_HEIGHT = 30;
    const int BUTTON_WIDTH = m_bounds.w - 20; // 10px padding on each side
    const int SPACING = 10;
    const int START_Y = TITLE_HEIGHT + 10;
    
    // Tool names and default selections
    struct ToolInfo {
        ToolType type;
        std::string name;
        bool defaultSelected;
    };
    
    ToolInfo tools[] = {
        {ToolType::Brush, "Brush", true},
        {ToolType::Line, "Line", false},
        {ToolType::Rectangle, "Rectangle", false},
        {ToolType::Circle, "Circle", false},
        {ToolType::Eraser, "Eraser", false},
        {ToolType::Fill, "Fill", false}
    };
    
    int index = 0;
    for (const auto& tool : tools) {
        int y = START_Y + index * (BUTTON_HEIGHT + SPACING);
        
        auto button = std::make_unique<Button>(
            m_bounds.x + 10, y, BUTTON_WIDTH, BUTTON_HEIGHT, tool.name
        );
        
        // Set initial selection state
        button->setSelected(tool.defaultSelected);
        if (tool.defaultSelected) {
            m_selectedTool = tool.type;
        }
        
        // Capture tool.type by value for the lambda
        ToolType type = tool.type;
        button->setOnClick([this, type]() {
            selectTool(type);
        });
        
        m_toolButtons.push_back(button.get());
        addChild(std::move(button));
        
        index++;
    }
    
    // Add brush size slider after the tools
    int sliderY = START_Y + index * (BUTTON_HEIGHT + SPACING) + SPACING;
    m_brushSizeSlider = new Slider(
        m_bounds.x + 10, sliderY, BUTTON_WIDTH, 20, "Brush Size", 0.25f
    );
    
    m_brushSizeSlider->setOnValueChanged([this](float value) {
        // Scale value to brush size range (1-20)
        m_brushSize = static_cast<int>(1.0f + value * 19.0f);
    });
    
    addChild(std::unique_ptr<UIElement>(m_brushSizeSlider));
}

void ToolboxPanel::selectTool(ToolType tool) {
    m_selectedTool = tool;
    
    // Update button selection states
    size_t index = 0;
    for (auto button : m_toolButtons) {
        button->setSelected(index == static_cast<size_t>(tool));
        index++;
    }
}

//
// UI static helper methods
//
void UI::drawText(const std::string& text, int x, int y, const SDL_Color& color) {
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    // Switch to orthographic projection for text rendering
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    
    // Get the current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    glOrtho(0, viewport[2], viewport[3], 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Simple text rendering using lines
    glColor4f(r, g, b, a);
    glLineWidth(1.5f);
    
    const int charWidth = 8;
    const int charHeight = 12;
    int currentX = x;
    
    for (char c : text) {
        // Simple representations of characters as lines
        glBegin(GL_LINES);
        switch (c) {
            case 'A':
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth/2, y);
                glVertex2f(currentX + charWidth/2, y);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                break;
                
            case 'B':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                break;
                
            case 'C':
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX, y);
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                break;
                
            case 'D':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y);
                glVertex2f(currentX + charWidth/2, y);
                glVertex2f(currentX + charWidth/2, y);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth/2, y + charHeight);
                glVertex2f(currentX + charWidth/2, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                break;
                
            case 'E':
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX, y);
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                break;
                
            case 'F':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                break;
                
            case 'G':
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y);
                glVertex2f(currentX, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth/2, y + charHeight/2);
                break;
                
            case 'H':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y + charHeight);
                break;
                
            case 'I':
                glVertex2f(currentX + charWidth/2, y);
                glVertex2f(currentX + charWidth/2, y + charHeight);
                break;
                
            case 'J':
                glVertex2f(currentX + charWidth/2, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX, y);
                break;
                
            case 'K':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight/2);
                glVertex2f(currentX + charWidth, y);
                break;
                
            case 'L':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y);
                glVertex2f(currentX + charWidth, y);
                break;
                
            case 'M':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth/2, y + charHeight/2);
                glVertex2f(currentX + charWidth/2, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y);
                break;
                
            case 'N':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y + charHeight);
                break;
                
            case 'O':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX, y);
                break;
                
            case 'P':
                glVertex2f(currentX, y);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX + charWidth, y + charHeight/2);
                glVertex2f(currentX, y + charHeight/2);
                break;
                
            // Handle other characters...
            default:
                // For unsupported characters, draw a simple box
                glVertex2f(currentX, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX + charWidth, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y + charHeight);
                glVertex2f(currentX, y);
                break;
        }
        glEnd();
        
        currentX += charWidth;
    }
    
    // Restore matrices
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void UI::drawRect(const SDL_Rect& rect, const SDL_Color& color, bool filled) {
    float r = color.r / 255.0f;
    float g = color.g / 255.0f;
    float b = color.b / 255.0f;
    float a = color.a / 255.0f;
    
    glColor4f(r, g, b, a);
    
    if (filled) {
        glBegin(GL_QUADS);
        glVertex2f(rect.x, rect.y);
        glVertex2f(rect.x + rect.w, rect.y);
        glVertex2f(rect.x + rect.w, rect.y + rect.h);
        glVertex2f(rect.x, rect.y + rect.h);
        glEnd();
    } else {
        glLineWidth(1.0f);
        glBegin(GL_LINE_LOOP);
        glVertex2f(rect.x, rect.y);
        glVertex2f(rect.x + rect.w, rect.y);
        glVertex2f(rect.x + rect.w, rect.y + rect.h);
        glVertex2f(rect.x, rect.y + rect.h);
        glEnd();
    }
}

//
// Main UI class implementation
//
UI::UI(int screenWidth, int screenHeight)
    : m_screenWidth(screenWidth)
    , m_screenHeight(screenHeight)
{
}

UI::~UI() {
    cleanup();
}

bool UI::initialize() {
    if (m_initialized) return true;
    
    // Create the root dock panel
    m_rootPanel = std::make_unique<DockPanel>(0, 0, m_screenWidth, m_screenHeight);
    
    // Create and dock the main panels
    auto scenePanel = std::make_unique<ScenePanel>(0, 0, 0, 0); // Size set by docking
    auto materialsPanel = std::make_unique<MaterialsPanel>(0, 0, 0, 0); // Size set by docking
    auto toolboxPanel = std::make_unique<ToolboxPanel>(0, 0, 0, 0); // Size set by docking
    
    // Store raw pointers for easy access
    m_scenePanel = scenePanel.get();
    m_materialsPanel = materialsPanel.get();
    m_toolboxPanel = toolboxPanel.get();
    
    // Dock the panels
    m_rootPanel->dockPanel(std::move(scenePanel), DockPanel::DockPosition::Center);
    m_rootPanel->dockPanel(std::move(materialsPanel), DockPanel::DockPosition::Right);
    m_rootPanel->dockPanel(std::move(toolboxPanel), DockPanel::DockPosition::Left);
    
    m_initialized = true;
    return true;
}

void UI::cleanup() {
    // Clear panels
    m_rootPanel.reset();
    
    // Reset panel pointers
    m_scenePanel = nullptr;
    m_materialsPanel = nullptr;
    m_toolboxPanel = nullptr;
    
    m_initialized = false;
}

bool UI::handleEvent(const SDL_Event& event) {
    if (!m_initialized || !m_rootPanel) return false;
    
    return m_rootPanel->handleEvent(event);
}

void UI::update() {
    if (!m_initialized || !m_rootPanel) return;
    
    m_rootPanel->update();
}

void UI::render() {
    if (!m_initialized || !m_rootPanel) return;
    
    // Clear any existing OpenGL errors
    while (glGetError() != GL_NO_ERROR) {}
    
    // Set up OpenGL for UI rendering
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Set up orthographic projection for UI
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0, m_screenWidth, m_screenHeight, 0, -1, 1);
    
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    
    // Render the UI hierarchy
    m_rootPanel->render();
    
    // Render FPS counter and status text at the bottom of the screen
    if (!m_statusText.empty()) {
        SDL_Color textColor = {220, 220, 220, 255};
        drawText(m_statusText, 10, m_screenHeight - 20, textColor);
    }
    
    std::string fpsText = "FPS: " + std::to_string(m_fps);
    SDL_Color fpsColor = {180, 180, 180, 255};
    drawText(fpsText, m_screenWidth - 60, m_screenHeight - 20, fpsColor);
    
    // Restore matrices
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
}

void UI::resize(int width, int height) {
    m_screenWidth = width;
    m_screenHeight = height;
    
    if (m_rootPanel) {
        m_rootPanel->setSize(width, height);
        // This will cascade down and resize all docked panels
    }
}

void UI::toggleFullscreen(SDL_Window* window) {
    m_isFullscreen = !m_isFullscreen;
    
    if (m_isFullscreen) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
    } else {
        SDL_SetWindowFullscreen(window, 0);
    }
    
    // Get the new window size
    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    
    // Resize the UI for the new window size
    resize(width, height);
}

MaterialType UI::getSelectedMaterial() const {
    return m_materialsPanel ? m_materialsPanel->getSelectedMaterial() : MaterialType::Sand;
}

ToolType UI::getSelectedTool() const {
    return m_toolboxPanel ? m_toolboxPanel->getSelectedTool() : ToolType::Brush;
}

int UI::getBrushSize() const {
    return m_toolboxPanel ? m_toolboxPanel->getBrushSize() : 5;
}

float UI::getZoomLevel() const {
    return m_scenePanel ? m_scenePanel->getZoomLevel() : 1.0f;
}

void UI::getCameraPosition(int& x, int& y) const {
    if (m_scenePanel) {
        m_scenePanel->getCameraPosition(x, y);
    } else {
        x = y = 0;
    }
}

SDL_Rect UI::getScenePanelRect() const {
    return m_scenePanel ? m_scenePanel->getBounds() : SDL_Rect{0, 0, 0, 0};
}

bool UI::isSimulationRunning() const {
    return m_scenePanel ? m_scenePanel->isSimulationRunning() : false;
}

} // namespace PixelPhys