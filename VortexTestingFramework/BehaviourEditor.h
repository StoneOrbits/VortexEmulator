#ifndef BEHAVIOUR_EDITOR_H
#define BEHAVIOUR_EDITOR_H

#include <Windows.h>
#include <stdint.h>

#include "VortexConfig.h"
#include "Behaviours/Behaviours.h"

class BehaviourEditor
{
public:

  BehaviourEditor();
  bool init(HINSTANCE inst, HWND parent, int x, int y, int w, int h);
  HWND hwnd();
  void redraw();

private:

  struct EditorNode
  {
    uint8_t behaviourIndex;
    int x;
    int y;
  };

  static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

  void draw(HDC dc);
  void onMouseDown(int x, int y);
  void onMouseUp(int x, int y);
  void onMouseMove(int x, int y);
  int findOutputSocket(int x, int y);
  void showContextMenu(int screenX, int screenY, int clientX, int clientY);
  int findNodeAt(int x, int y);

  static const char *nodeTypeName(Behaviours::NodeType t);
  static COLORREF nodeColor(Behaviours::NodeType t);

private:

  HINSTANCE m_inst;
  HWND m_hwnd;

  EditorNode m_nodes[MAX_BEHAVIOUR_NODES];
  uint8_t m_nodeCount;

  bool m_dragging;
  int m_dragNode;
  int m_dragOffsetX;
  int m_dragOffsetY;

  bool m_linking;
  int m_linkNode;
  int m_linkMouseX;
  int m_linkMouseY;
};

#endif