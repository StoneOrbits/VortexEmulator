#include "BehaviourEditor.h"
#include "Behaviours/BehaviourNode.h"

#include <string>
#include <stdio.h>

#define NODE_W 150
#define NODE_H 80
#define SOCKET_R 6

BehaviourEditor::BehaviourEditor() :
  m_inst(nullptr),
  m_hwnd(NULL),
  m_nodes{},
  m_nodeCount(0),
  m_dragging(false),
  m_dragNode(0),
  m_dragOffsetX(0),
  m_dragOffsetY(0),
  m_linking(false),
  m_linkNode(0),
  m_linkMouseX(0),
  m_linkMouseY(0),
  m_edit(NULL),
  m_editNode(-1),
  m_editParam(-1)
{
}

void BehaviourEditor::populateFromBehaviours()
{
  m_nodeCount = 0;

  uint8_t totalNodes = Behaviours::nodeCount();
  for (uint8_t i = 0; i < totalNodes; i++) {
    BehaviourNode *bn = Behaviours::node(i);
    if (!bn) continue;

    EditorNode &en = m_nodes[m_nodeCount++];
    en.behaviourIndex = i;

    int column = 0;
    switch (Behaviours::getSubtype(bn->type())) {
    case Behaviours::SUBTYPE_INPUT: column = 0; break;
    case Behaviours::SUBTYPE_MODIFIER: column = 1; break;
    case Behaviours::SUBTYPE_FUNCTIONAL: column = 2; break;
    default: column = 1; break;
    }

    int xSpacing = 200;
    int ySpacing = 100;

    int columnCount = 0;
    for (uint8_t j = 0; j < m_nodeCount - 1; j++) {
      BehaviourNode *other = Behaviours::node(m_nodes[j].behaviourIndex);
      if (Behaviours::getSubtype(other->type()) == Behaviours::getSubtype(bn->type()))
        columnCount++;
    }

    en.x = 50 + column * xSpacing;
    en.y = 50 + columnCount * ySpacing;
  }

  redraw();
}

bool BehaviourEditor::init(HINSTANCE inst, HWND parent, int x, int y, int w, int h)
{
  m_inst = inst;

  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = BehaviourEditor::wndproc;
  wc.hInstance = inst;
  wc.lpszClassName = "BehaviourEditorWnd";
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);

  RegisterClass(&wc);

  m_hwnd = CreateWindowEx(
    WS_EX_APPWINDOW,
    "BehaviourEditorWnd",
    "Behaviour Node Editor",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    x, y, w, h,
    parent,
    NULL,
    inst,
    this
  );

  return m_hwnd != NULL;
}

HWND BehaviourEditor::hwnd()
{
  return m_hwnd;
}

void BehaviourEditor::redraw()
{
  InvalidateRect(m_hwnd, NULL, TRUE);
}

int BehaviourEditor::findNodeAt(int x, int y)
{
  for (int i = m_nodeCount - 1; i >= 0; i--) {
    EditorNode &n = m_nodes[i];
    if (x >= n.x && x <= n.x + NODE_W &&
      y >= n.y && y <= n.y + NODE_H)
      return i;
  }
  return -1;
}

int BehaviourEditor::findParamHit(int x, int y, int &paramIndex)
{
  for (int i = 0; i < m_nodeCount; i++) {
    EditorNode &n = m_nodes[i];

    RECT p0 = { n.x + 30, n.y + 22, n.x + NODE_W - 10, n.y + 36 };
    RECT p1 = { n.x + 30, n.y + 40, n.x + NODE_W - 10, n.y + 56 };

    if (x >= p0.left && x <= p0.right && y >= p0.top && y <= p0.bottom) {
      paramIndex = 0;
      return i;
    }

    if (x >= p1.left && x <= p1.right && y >= p1.top && y <= p1.bottom) {
      paramIndex = 1;
      return i;
    }
  }

  return -1;
}

void BehaviourEditor::beginParamEdit(int nodeIndex, int paramIndex)
{
  if (m_edit)
    DestroyWindow(m_edit);

  EditorNode &en = m_nodes[nodeIndex];
  BehaviourNode *bn = Behaviours::node(en.behaviourIndex);

  float value = paramIndex == 0 ? bn->param1 : bn->param2;

  int x = en.x + 30;
  int y = en.y + (paramIndex == 0 ? 22 : 40);
  int w = 60;
  int h = 16;

  char buf[32];
  sprintf_s(buf, sizeof(buf), "%.3f", value);

  m_edit = CreateWindowExA(
    0,
    "EDIT",
    buf,
    WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
    x,
    y,
    w,
    h,
    m_hwnd,
    NULL,
    m_inst,
    NULL
  );

  SendMessage(m_edit, EM_SETSEL, 0, -1);
  SetFocus(m_edit);

  m_editNode = nodeIndex;
  m_editParam = paramIndex;
}

int BehaviourEditor::findOutputSocket(int x, int y)
{
  for (int i = 0; i < m_nodeCount; i++) {
    EditorNode &n = m_nodes[i];
    int sx = n.x + NODE_W;
    int sy = n.y + NODE_H / 2;
    int dx = x - sx;
    int dy = y - sy;
    if (dx * dx + dy * dy <= SOCKET_R * SOCKET_R * 4)
      return i;
  }
  return -1;
}

int BehaviourEditor::findInputSocket(int x, int y, int &socketIndex)
{
  for (int i = 0; i < m_nodeCount; i++) {
    EditorNode &n = m_nodes[i];
    BehaviourNode *bn = Behaviours::node(n.behaviourIndex);

    uint8_t numSockets = bn->inputCount();
    if (numSockets == 0 && bn->type() != Behaviours::NODE_ACCEL_MOTION &&
      bn->type() != Behaviours::NODE_ACCEL_NORMALIZED) {
      numSockets = 1;
    }

    for (uint8_t j = 0; j < numSockets; j++) {
      int sx = n.x - SOCKET_R * 2;
      int sy = n.y + NODE_H / 2 - SOCKET_R + (4 * SOCKET_R * j);

      int dx = x - sx;
      int dy = y - sy;

      if (dx * dx + dy * dy <= SOCKET_R * SOCKET_R * 4) {
        socketIndex = j;
        return i;
      }
    }
  }
  return -1;
}

void BehaviourEditor::onMouseDown(int x, int y)
{
  int paramIndex = -1;
  int paramNode = findParamHit(x, y, paramIndex);

  if (paramNode != -1) {
    beginParamEdit(paramNode, paramIndex);
    return;
  }

  int sock = findOutputSocket(x, y);
  if (sock != -1) {
    m_linking = true;
    m_linkNode = sock;
    m_linkMouseX = x;
    m_linkMouseY = y;
    redraw();
    return;
  }

  int node = findNodeAt(x, y);
  if (node != -1) {

    if (m_edit) {
      DestroyWindow(m_edit);
      m_edit = NULL;
    }

    m_dragging = true;
    m_dragNode = node;
    m_dragOffsetX = x - m_nodes[node].x;
    m_dragOffsetY = y - m_nodes[node].y;
  }
}

void BehaviourEditor::onMouseUp(int x, int y)
{
  if (!m_linking) {
    m_dragging = false;
    return;
  }

  int inputIndex = -1;
  int targetNode = findInputSocket(x, y, inputIndex);

  if (targetNode != -1 && inputIndex != -1 && targetNode != m_linkNode) {
    BehaviourNode *src = Behaviours::node(m_nodes[m_linkNode].behaviourIndex);
    BehaviourNode *dst = Behaviours::node(m_nodes[targetNode].behaviourIndex);
    dst->addInput(src);
  }

  m_linking = false;
  redraw();
}

void BehaviourEditor::onMouseMove(int x, int y)
{
  if (m_linking) {
    m_linkMouseX = x;
    m_linkMouseY = y;
    redraw();
    return;
  }

  if (!m_dragging)
    return;

  EditorNode &n = m_nodes[m_dragNode];

  n.x = x - m_dragOffsetX;
  n.y = y - m_dragOffsetY;

  if (m_edit && m_dragNode == m_editNode) {
    int ex = n.x + 30;
    int ey = n.y + (m_editParam == 0 ? 22 : 40);
    SetWindowPos(m_edit, NULL, ex, ey, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
  }

  redraw();
}

void BehaviourEditor::showContextMenu(int screenX, int screenY, int clientX, int clientY)
{
  HMENU menu = CreatePopupMenu();
  struct MenuItem
  {
    uint32_t id;
    Behaviours::NodeType type;
    const char *name;
  };

  MenuItem items[] = {
      {1, Behaviours::NODE_ACCEL_MOTION, "Accel Motion"},
      {2, Behaviours::NODE_ACCEL_NORMALIZED, "Accel Normalized"},
      {3, Behaviours::NODE_ACCEL_CURVED, "Accel Curved"},
      {4, Behaviours::NODE_ACCEL_FILTERED, "Accel Filtered"},
      {5, Behaviours::NODE_ACCEL_DIR_X, "Accel Dir X"},
      {6, Behaviours::NODE_ACCEL_DIR_Y, "Accel Dir Y"},
      {7, Behaviours::NODE_ACCEL_DIR_Z, "Accel Dir Z"},
      {8, Behaviours::NODE_ACCEL_TILT, "Accel Tilt"},
      {9, Behaviours::NODE_ABS, "Abs"},
      {10, Behaviours::NODE_ADD, "Add"},
      {11, Behaviours::NODE_MULTIPLY, "Multiply"},
      {12, Behaviours::NODE_CLAMP, "Clamp"},
      {13, Behaviours::NODE_CURVE, "Curve"},
      {14, Behaviours::NODE_THRESHOLD, "Threshold"},
      {15, Behaviours::NODE_MODE_BLEND, "Mode Blend"}
  };

  for (auto &i : items) AppendMenu(menu, MF_STRING, i.id, i.name);

  int cmd = TrackPopupMenu(menu, TPM_RETURNCMD, screenX, screenY, 0, m_hwnd, NULL);
  DestroyMenu(menu);
  if (cmd == 0) return;

  Behaviours::NodeType type = Behaviours::NODE_ADD;
  for (auto &i : items) if (i.id == (uint32_t)cmd) type = i.type;

  uint8_t idx = Behaviours::create(type);
  if (idx == 255) return;

  EditorNode &n = m_nodes[m_nodeCount++];
  n.behaviourIndex = idx;
  n.x = clientX;
  n.y = clientY;
  redraw();
}

void BehaviourEditor::draw(HDC dc)
{
  RECT rc;
  GetClientRect(m_hwnd, &rc);

  HBRUSH bg = CreateSolidBrush(RGB(35, 35, 35));
  FillRect(dc, &rc, bg);
  DeleteObject(bg);

  SetBkMode(dc, TRANSPARENT);
  SetTextColor(dc, RGB(255, 255, 255));

  HPEN pen = CreatePen(PS_SOLID, 2, RGB(200, 200, 200));
  HPEN oldPen = (HPEN)SelectObject(dc, pen);

  for (int i = 0; i < m_nodeCount; i++) {
    BehaviourNode *dstNode = Behaviours::node(m_nodes[i].behaviourIndex);

    for (uint8_t j = 0; j < dstNode->inputCount(); j++) {
      BehaviourNode *srcNode = dstNode->input(j);
      if (!srcNode) continue;

      int srcIndex = -1;

      for (int k = 0; k < m_nodeCount; k++) {
        if (Behaviours::node(m_nodes[k].behaviourIndex) == srcNode) {
          srcIndex = k;
          break;
        }
      }

      if (srcIndex == -1) continue;

      int x1 = m_nodes[srcIndex].x + NODE_W;
      int y1 = m_nodes[srcIndex].y + NODE_H / 2;

      int x2 = m_nodes[i].x;
      int y2 = m_nodes[i].y + 40 + (4 * j * SOCKET_R);

      MoveToEx(dc, x1, y1, NULL);
      LineTo(dc, x2, y2);
    }
  }

  if (m_linking && m_linkNode >= 0 && m_linkNode < m_nodeCount) {
    int x1 = m_nodes[m_linkNode].x + NODE_W;
    int y1 = m_nodes[m_linkNode].y + NODE_H / 2;

    MoveToEx(dc, x1, y1, NULL);
    LineTo(dc, m_linkMouseX, m_linkMouseY);
  }

  SelectObject(dc, oldPen);
  DeleteObject(pen);

  for (uint8_t i = 0; i < m_nodeCount; i++) {

    EditorNode &en = m_nodes[i];
    BehaviourNode *bn = Behaviours::node(en.behaviourIndex);

    RECT r = { en.x, en.y, en.x + NODE_W, en.y + NODE_H };

    COLORREF color = nodeColor(bn->type());

    HBRUSH br = CreateSolidBrush(color);
    HBRUSH oldBrush = (HBRUSH)SelectObject(dc, br);

    Rectangle(dc, r.left, r.top, r.right, r.bottom);

    SelectObject(dc, oldBrush);
    DeleteObject(br);

    const char *name = nodeTypeName(bn->type());
    TextOutA(dc, r.left + 6, r.top + 4, name, strlen(name));

    char buf[64];

    sprintf_s(buf, sizeof(buf), "p0 %.2f", bn->param1);
    TextOutA(dc, r.left + 12, r.top + 22, buf, strlen(buf));

    sprintf_s(buf, sizeof(buf), "p1 %.2f", bn->param2);
    TextOutA(dc, r.left + 12, r.top + 42, buf, strlen(buf));

    uint8_t numInputs = bn->inputCount();

    for (uint8_t j = 0; j < numInputs; j++) {
      int sx = r.left - SOCKET_R * 2;
      int sy = r.top + NODE_H / 2 - SOCKET_R + (4 * SOCKET_R * j);

      Ellipse(dc,
        sx,
        sy,
        sx + SOCKET_R * 2,
        sy + SOCKET_R * 2);
    }

    int ox = r.right;
    int oy = r.top + NODE_H / 2 - SOCKET_R;

    Ellipse(dc,
      ox,
      oy,
      ox + SOCKET_R * 2,
      oy + SOCKET_R * 2);
  }
}

LRESULT CALLBACK BehaviourEditor::wndproc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  BehaviourEditor *ed;

  if (msg == WM_NCCREATE) {
    CREATESTRUCT *cs = (CREATESTRUCT *)lParam;
    ed = (BehaviourEditor *)cs->lpCreateParams;
    SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)ed);
    return TRUE;
  }

  ed = (BehaviourEditor *)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!ed) return DefWindowProc(hwnd, msg, wParam, lParam);

  switch (msg) {

  case WM_COMMAND:
  {
    if ((HWND)lParam == ed->m_edit && HIWORD(wParam) == EN_KILLFOCUS) {

      char buf[64];
      GetWindowTextA(ed->m_edit, buf, sizeof(buf));

      float v = (float)atof(buf);
      if (v < 0.0f) v = 0.0f;
      if (v > 1.0f) v = 1.0f;

      BehaviourNode *bn = Behaviours::node(ed->m_nodes[ed->m_editNode].behaviourIndex);

      if (ed->m_editParam == 0)
        bn->param1 = v;
      else
        bn->param2 = v;

      DestroyWindow(ed->m_edit);
      ed->m_edit = NULL;

      ed->redraw();
    }
    break;
  }

  case WM_LBUTTONDOWN:
    ed->onMouseDown(LOWORD(lParam), HIWORD(lParam));
    return 0;

  case WM_LBUTTONUP:
    ed->onMouseUp(LOWORD(lParam), HIWORD(lParam));
    return 0;

  case WM_MOUSEMOVE:
    ed->onMouseMove(LOWORD(lParam), HIWORD(lParam));
    return 0;

  case WM_RBUTTONUP:
  {
    POINT screen; screen.x = LOWORD(lParam); screen.y = HIWORD(lParam);
    ClientToScreen(hwnd, &screen);
    ed->showContextMenu(screen.x, screen.y, LOWORD(lParam), HIWORD(lParam));
    return 0;
  }

  case WM_PAINT:
  {
    PAINTSTRUCT ps;
    HDC dc = BeginPaint(hwnd, &ps);
    ed->draw(dc);
    EndPaint(hwnd, &ps);
    return 0;
  }

  }

  return DefWindowProc(hwnd, msg, wParam, lParam);
}

COLORREF BehaviourEditor::nodeColor(Behaviours::NodeType t)
{
  switch (t) {
  case Behaviours::NODE_ACCEL_MOTION:
  case Behaviours::NODE_ACCEL_NORMALIZED:
  case Behaviours::NODE_ACCEL_CURVED:
  case Behaviours::NODE_ACCEL_FILTERED:
  case Behaviours::NODE_ACCEL_DIR_X:
  case Behaviours::NODE_ACCEL_DIR_Y:
  case Behaviours::NODE_ACCEL_DIR_Z:
  case Behaviours::NODE_ACCEL_TILT: return RGB(70, 120, 200);

  case Behaviours::NODE_ADD:
  case Behaviours::NODE_MULTIPLY:
  case Behaviours::NODE_ABS: return RGB(200, 120, 70);

  case Behaviours::NODE_CLAMP:
  case Behaviours::NODE_CURVE:
  case Behaviours::NODE_THRESHOLD: return RGB(120, 200, 120);

  case Behaviours::NODE_MODE_BLEND: return RGB(200, 80, 200);
  }

  return RGB(80, 80, 80);
}

const char *BehaviourEditor::nodeTypeName(Behaviours::NodeType t)
{
  switch (t) {
  case Behaviours::NODE_ACCEL_MOTION: return "Accel Motion";
  case Behaviours::NODE_ACCEL_NORMALIZED: return "Accel Norm";
  case Behaviours::NODE_ACCEL_CURVED: return "Accel Curve";
  case Behaviours::NODE_ACCEL_FILTERED: return "Accel Filter";
  case Behaviours::NODE_ACCEL_DIR_X: return "Accel X";
  case Behaviours::NODE_ACCEL_DIR_Y: return "Accel Y";
  case Behaviours::NODE_ACCEL_DIR_Z: return "Accel Z";
  case Behaviours::NODE_ACCEL_TILT: return "Tilt";
  case Behaviours::NODE_ABS: return "Abs";
  case Behaviours::NODE_ADD: return "Add";
  case Behaviours::NODE_MULTIPLY: return "Multiply";
  case Behaviours::NODE_CLAMP: return "Clamp";
  case Behaviours::NODE_CURVE: return "Curve";
  case Behaviours::NODE_THRESHOLD: return "Threshold";
  case Behaviours::NODE_MODE_BLEND: return "Blend";
  }

  return "Node";
}