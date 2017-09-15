// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2014 Henner Zeller <h.zeller@acm.org>
// Copyright (C) 2015 Christoph Friedrich <christoph.friedrich@vonaffenfels.de>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#include <assert.h>
#include <stdio.h>

#include "transformer.h"

namespace rgb_matrix {

/*****************************/
/* Rotate Transformer Canvas */
/*****************************/
class RotateTransformer::TransformCanvas : public Canvas {
public:
  TransformCanvas(int angle);

  void SetDelegatee(Canvas* delegatee);
  void SetAngle(int angle);

  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);
  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);

private:
  Canvas *delegatee_;
  int angle_;
};

RotateTransformer::TransformCanvas::TransformCanvas(int angle)
  : delegatee_(NULL) {
  SetAngle(angle);
}

void RotateTransformer::TransformCanvas::SetDelegatee(Canvas* delegatee) {
  delegatee_ = delegatee;
}

void RotateTransformer::TransformCanvas::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
  switch (angle_) {
  case 0:
    delegatee_->SetPixel(x, y, red, green, blue);
    break;
  case 90:
    delegatee_->SetPixel(delegatee_->width() - y - 1, x,
                         red, green, blue);
    break;
  case 180:
    delegatee_->SetPixel(delegatee_->width() - x - 1,
                         delegatee_->height() - y - 1,
                         red, green, blue);
    break;
  case 270:
    delegatee_->SetPixel(y, delegatee_->height() - x - 1, red, green, blue);
    break;
  }
}

int RotateTransformer::TransformCanvas::width() const {
  return (angle_ % 180 == 0) ? delegatee_->width() : delegatee_->height();
}

int RotateTransformer::TransformCanvas::height() const {
  return (angle_ % 180 == 0) ? delegatee_->height() : delegatee_->width();
}

void RotateTransformer::TransformCanvas::Clear() {
  delegatee_->Clear();
}

void RotateTransformer::TransformCanvas::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  delegatee_->Fill(red, green, blue);
}

void RotateTransformer::TransformCanvas::SetAngle(int angle) {
  assert(angle % 90 == 0);  // We currenlty enforce that for more pretty output
  angle_ = (angle + 360) % 360;
}

/**********************/
/* Rotate Transformer */
/**********************/
RotateTransformer::RotateTransformer(int angle)
  : angle_(angle), canvas_(new TransformCanvas(angle)) {
}

RotateTransformer::~RotateTransformer() {
  delete canvas_;
}

Canvas *RotateTransformer::Transform(Canvas *output) {
  assert(output != NULL);

  canvas_->SetDelegatee(output);
  return canvas_;
}

void RotateTransformer::SetAngle(int angle) {
  canvas_->SetAngle(angle);
  angle_ = angle;
}

/**********************/
/* Linked Transformer */
/**********************/
void LinkedTransformer::AddTransformer(CanvasTransformer *transformer) {
  list_.push_back(transformer);
}

void LinkedTransformer::AddTransformer(List transformer_list) {
  list_.insert(list_.end(), transformer_list.begin(), transformer_list.end());
}
void LinkedTransformer::SetTransformer(List transformer_list) {
  list_ = transformer_list;
}

Canvas *LinkedTransformer::Transform(Canvas *output) {
  for (size_t i = 0; i < list_.size(); ++i) {
    output = list_[i]->Transform(output);
  }

  return output;
}

void LinkedTransformer::DeleteTransformers() {
  for (size_t i = 0; i < list_.size(); ++i) {
    delete list_[i];
  }
  list_.clear();
}

// U-Arrangement Transformer.
class UArrangementTransformer::TransformCanvas : public Canvas {
public:
  TransformCanvas(int parallel) : parallel_(parallel), delegatee_(NULL) {}

  void SetDelegatee(Canvas* delegatee);

  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);
  virtual int width() const { return width_; }
  virtual int height() const { return height_; }
  virtual void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);

private:
  const int parallel_;
  int width_;
  int height_;
  int panel_height_;
  Canvas *delegatee_;
};

void UArrangementTransformer::TransformCanvas::SetDelegatee(Canvas* delegatee) {
  delegatee_ = delegatee;
  width_ = (delegatee->width() / 64) * 32;   // Div in middle at 32px boundary
  height_ = 2 * delegatee->height();
  if (delegatee->width() % 64 != 0) {
    fprintf(stderr, "An U-arrangement would need an even number of panels "
            "unless you can fold one in the middle...\n");
  }
  if (delegatee->height() % parallel_ != 0) {
    fprintf(stderr, "For parallel=%d we would expect the height=%d to be "
            "divisible by %d ??\n", parallel_, delegatee->height(), parallel_);
    assert(false);
  }
  panel_height_ = delegatee->height() / parallel_;
}

void UArrangementTransformer::TransformCanvas::Clear() {
  delegatee_->Clear();
}

void UArrangementTransformer::TransformCanvas::Fill(
  uint8_t red, uint8_t green, uint8_t blue) {
  delegatee_->Fill(red, green, blue);
}

void UArrangementTransformer::TransformCanvas::SetPixel(
  int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
  if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
  const int slab_height = 2*panel_height_;   // one folded u-shape
  const int base_y = (y / slab_height) * panel_height_;
  y %= slab_height;
  if (y < panel_height_) {
    x += delegatee_->width() / 2;
  } else {
    x = width_ - x - 1;
    y = slab_height - y - 1;
  }
  delegatee_->SetPixel(x, base_y + y, red, green, blue);
}

UArrangementTransformer::UArrangementTransformer(int parallel)
  : canvas_(new TransformCanvas(parallel)) {
  assert(parallel > 0);
}

UArrangementTransformer::~UArrangementTransformer() {
  delete canvas_;
}

Canvas *UArrangementTransformer::Transform(Canvas *output) {
  assert(output != NULL);

  canvas_->SetDelegatee(output);
  return canvas_;
}

// Legacly LargeSquare64x64Transformer: uses the UArrangementTransformer, but
// does things so that it looks the same as before.
LargeSquare64x64Transformer::LargeSquare64x64Transformer()
  : arrange_(1), rotated_(180) { }
Canvas *LargeSquare64x64Transformer::Transform(Canvas *output) {
  return rotated_.Transform(arrange_.Transform(output));
}


/********************************/
/* P10outdoor Transformer Canvas */
/********************************/
class P10outdoorTransformer::TransformCanvas : public Canvas {
public:
  TransformCanvas() : delegatee_(NULL) {}

  void SetDelegatee(Canvas* delegatee);

  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);
  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);

private:
  Canvas *delegatee_;
};

void P10outdoorTransformer::TransformCanvas::SetDelegatee(Canvas* delegatee) {
  delegatee_ = delegatee;
}

void P10outdoorTransformer::TransformCanvas::Clear() {
  delegatee_->Clear();
}

void P10outdoorTransformer::TransformCanvas::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  delegatee_->Fill(red, green, blue);
}

int P10outdoorTransformer::TransformCanvas::width() const {
  return delegatee_->width() / 4;
}

int P10outdoorTransformer::TransformCanvas::height() const {
  return delegatee_->height() * 2;
}

void P10outdoorTransformer::TransformCanvas::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
  int new_x = x;
  int new_y = y;
  int panel_x = x / 32;
  int x_offset = 0;
  switch (y % 8){
    case 0: x_offset = 64;break;
    case 1: x_offset = 64;break;
    case 2: x_offset = 72;break;
    case 3: x_offset = 72;break;
    case 4: x_offset = 0;break;
    case 5: x_offset = 0;break;
    case 6: x_offset = 8;break;
    case 7: x_offset = 8;break;
  }

  new_y = 4 * (y / 8) + (y % 2);

  if ((y % 4) < 2) {  //countdown
    new_x = ((x % 32) * -1) + (24 * (((x % 32) / 8) + 1)) - 17;
  } else {
    new_x = (x % 8) + (16 * ((x % 32) / 8));
  }
  new_x += x_offset;
  new_x += (panel_x * 128);
  delegatee_->SetPixel(new_x, new_y, red, green, blue);
}
/*************************/
/* P10outdoor Transformer */
/*************************/
P10outdoorTransformer::P10outdoorTransformer()
  : canvas_(new TransformCanvas()) {
}

P10outdoorTransformer::~P10outdoorTransformer() {
  delete canvas_;
}

Canvas *P10outdoorTransformer::Transform(Canvas *output) {
  assert(output != NULL);

  canvas_->SetDelegatee(output);
  return canvas_;
}

/****************************/
/* Electrodragon transformer */
/****************************/

class EDTransformer::TransformCanvas : public Canvas {
public:
  TransformCanvas() : delegatee_(NULL) {}

  void SetDelegatee(Canvas* delegatee);

  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);
  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);

private:
  Canvas *delegatee_;
};

void EDTransformer::TransformCanvas::SetDelegatee(Canvas* delegatee) {
  delegatee_ = delegatee;
}

void EDTransformer::TransformCanvas::Clear() {
  delegatee_->Clear();
}

void EDTransformer::TransformCanvas::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  delegatee_->Fill(red, green, blue);
}

int EDTransformer::TransformCanvas::width() const {
  return delegatee_->width() / 2;
}

int EDTransformer::TransformCanvas::height() const {
  return delegatee_->height();
}

void EDTransformer::TransformCanvas::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
  	int new_x = x;
	int new_y = y;
  
	new_x = ((x / 16) * 32) + (x % 16);
	if ((y/8)%2 == 1) {
		new_x += 16; 
	}
	new_y = ((y / 16) * 16) + (y % 8); 	
	delegatee_->SetPixel(new_x, new_y, red, green, blue);
    
}
/*************************/
/* P10outdoor Transformer */
/*************************/
EDTransformer::EDTransformer()
  : canvas_(new TransformCanvas()) {
}

EDTransformer::~EDTransformer() {
  delete canvas_;
}

Canvas *EDTransformer::Transform(Canvas *output) {
  assert(output != NULL);

  canvas_->SetDelegatee(output);
  return canvas_;
}


/*********************************/
/*  CRAZy Chinese Transformer    */
/*********************************/

class CrazyChineseTransformer ::TransformCanvas : public Canvas {
public:
  TransformCanvas() : delegatee_(NULL) {}

  void SetDelegatee(Canvas* delegatee);

  virtual void Clear();
  virtual void Fill(uint8_t red, uint8_t green, uint8_t blue);
  virtual int width() const;
  virtual int height() const;
  virtual void SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue);

private:
  Canvas *delegatee_;
};

void CrazyChineseTransformer ::TransformCanvas::SetDelegatee(Canvas* delegatee) {
  delegatee_ = delegatee;
}

void CrazyChineseTransformer ::TransformCanvas::Clear() {
  delegatee_->Clear();
}

void CrazyChineseTransformer ::TransformCanvas::Fill(uint8_t red, uint8_t green, uint8_t blue) {
  delegatee_->Fill(red, green, blue);
}

int CrazyChineseTransformer ::TransformCanvas::width() const {
  return delegatee_->width() / 2;
}

int CrazyChineseTransformer ::TransformCanvas::height() const {
  return delegatee_->height() * 2;
}

void CrazyChineseTransformer ::TransformCanvas::SetPixel(int x, int y, uint8_t red, uint8_t green, uint8_t blue) {
 
	int new_x = 0;
	int new_y = 0;
	new_x = ((x / 32) * 64) + (x % 32);
	if ((y & 8) == 0) {
		new_x += 32;
	}  
	new_y = ((y / 16) * 8) + (y % 8);
	delegatee_->SetPixel(new_x, new_y, red, green, blue);
	//fprintf(stdout, "value of new_x new_y :: %d %d\n ", new_x, new_y);
}
/*************************/
/* P10outdoor Transformer */
/*************************/
CrazyChineseTransformer ::CrazyChineseTransformer ()
  : canvas_(new TransformCanvas()) {
}

CrazyChineseTransformer ::~CrazyChineseTransformer () {
  delete canvas_;
}

Canvas *CrazyChineseTransformer ::Transform(Canvas *output) {
  assert(output != NULL);

  canvas_->SetDelegatee(output);
  return canvas_;
}




} // namespace rgb_matrix

